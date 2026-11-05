// =====================================================================
// HDR → LDR の境目：Bloomの合成 ＋ トーンマッピング
//
//   ここより前はHDR（1.0を超える値を持つ）、ここより後はLDR（0〜1）。
//   ・Bloomは「光の量」を足す処理なので、HDRのうちに足す
//   ・その後でトーンマッピングし、0〜1に収める
//   ガンマ補正はここでは行わない（LDRのポストプロセスの後、ScreenBlitで最後に行う）
//
//   トーンマッピングの直後にカラーグレーディング（3D LUT）を読む。
//   ・FXAA より前：FXAA は「出来上がった見た目」の明るさで輪郭を探すべきなので、絵を決める処理は先に済ませる
//   ・同じパスの中：全画面の読み書きを1往復増やさずに済む
// =====================================================================

cbuffer PostProcessConfig : register(b5)
{
    float g_Exposure;
    float g_GammaCorrection; // ScreenBlit用（ここでは未使用）
    int g_Tonemapper; // 0=Exposure、1=ACES（C++の enum Tonemapper の番号）
    float g_BloomIntensity; // Bloom OFFのときは0が送られてくる
    int g_LutEnabled; // 1ならLUTを読む（C++の LutMode が Off 以外）
    float g_LutSize; // LUTの一辺の格子数（32）
    float2 g_PostPadding;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = float4(input.Position, 1.0);
    output.TexCoord = input.TexCoord;
    return output;
}

Texture2D sceneTexture : register(t0); // HDRのシーン（Resolve済み）
Texture2D bloomTexture : register(t1); // Bloomの最終結果（半解像度。未バインドなら0が読める）
Texture3D gradingLut : register(t2); // カラーグレーディングの LUT（入出力ともガンマ補正後の空間）
SamplerState linearSampler : register(s0); // LINEAR・CLAMP（LUT の端の外を読んでも端の値になる）

// ACES Filmic（Krzysztof Narkowicz の近似式）
float3 ACESFilm(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 scene = sceneTexture.Sample(linearSampler, input.TexCoord);
    float3 bloom = bloomTexture.Sample(linearSampler, input.TexCoord).rgb;

    // 1. HDRのままBloomを足す
    float3 hdr = scene.rgb + bloom * g_BloomIntensity;

    // 2. トーンマッピング（露出を掛けてからカーブに通す）
    float3 ldr;
    if (g_Tonemapper == 0) // Exposure
    {
        ldr = 1.0 - exp(-hdr * g_Exposure);
    }
    else
    {
        ldr = ACESFilm(hdr * g_Exposure);
    }

    ldr = saturate(ldr);

    // 3. カラーグレーディング（3D LUT）
    if (g_LutEnabled != 0)
    {
        // 3-1. 表の軸はガンマ補正後の空間で切ってある（リニアのまま等間隔に切ると暗部の区切りが粗くなる）。
        //      ScreenBlit と同じ pow 1/2.2 で直す（正確な sRGB の式にしないのは、ScreenBlit とそろえるため）
        const float3 encoded = pow(ldr, 1.0 / 2.2);

        // 3-2. 色 → UVW：0 と 1 が端の格子点の「中心」に当たるように、(N-1)/N に縮めて半マスずらす
        const float3 uvw = encoded * ((g_LutSize - 1.0) / g_LutSize) + 0.5 / g_LutSize;

        // 3-3. 1回読むだけで、R・G・B の三方向の補間をハードウェアが行う。ミップは無いので 0 を指定
        const float3 graded = gradingLut.SampleLevel(linearSampler, uvw, 0).rgb;

        // 3-4. リニアに戻す（戻さないと ScreenBlit でガンマが2回かかる）
        ldr = pow(graded, 2.2);
    }

    return float4(ldr, scene.a);
}
