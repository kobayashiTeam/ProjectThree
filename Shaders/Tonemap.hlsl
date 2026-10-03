// =====================================================================
// HDR → LDR の境目：Bloomの合成 ＋ トーンマッピング
//
//   ここより前はHDR（1.0を超える値を持つ）、ここより後はLDR（0〜1）。
//   ・Bloomは「光の量」を足す処理なので、HDRのうちに足す
//   ・その後でトーンマッピングし、0〜1に収める
//   ガンマ補正はここでは行わない（LDRのポストプロセスの後、ScreenBlitで最後に行う）
// =====================================================================

cbuffer PostProcessConfig : register(b5)
{
    float g_Exposure;
    float g_GammaCorrection; // ScreenBlit用（ここでは未使用）
    float g_Tonemapper; // 0.0=Exposure、1.0=ACES
    float g_BloomIntensity; // Bloom OFFのときは0が送られてくる
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
SamplerState linearSampler : register(s0);

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
    if (g_Tonemapper < 0.5)
    {
        ldr = 1.0 - exp(-hdr * g_Exposure);
    }
    else
    {
        ldr = ACESFilm(hdr * g_Exposure);
    }

    return float4(saturate(ldr), scene.a);
}
