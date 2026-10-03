// =====================================================================
// Bloom：縮小パス（13タップ・ダウンサンプル）
//   Jimenez, "Next Generation Post Processing in Call of Duty: Advanced Warfare" (SIGGRAPH 2014)
//
//   1回の描画で、読み込み元の半分の解像度へ縮小する。
//   縮小しながら周囲13点を重み付きで平均するので、縮小と同時に軽くぼける。
//   最初の1回だけ（isFirstPass = 1）次の2つを行う：
//     ・しきい値処理（ソフトニー）：しきい値を超えた分だけを残す
//     ・Karis平均：明るすぎる1ピクセル（ホタル）が縮小後に点滅するのを抑える
// =====================================================================

cbuffer BloomParams : register(b9)
{
    float2 g_SrcTexelSize; // 読み込み元の1テクセルのUV幅
    float g_Threshold;
    float g_Knee;
    float g_IsFirstPass;
    float g_FilterRadius; // 拡大パス用（ここでは未使用）
    float2 g_Padding;
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

Texture2D srcTexture : register(t0);
SamplerState linearClamp : register(s0);

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

// Karis平均の重み：明るいグループほど重みを小さくする（1 / (1 + 輝度)）
float KarisWeight(float3 c)
{
    return 1.0 / (1.0 + Luminance(c));
}

// ソフトニー付きのしきい値処理
//   しきい値 - knee から徐々に効き始め、しきい値 + knee 以降は「輝度 - しきい値」と同じになる。
//   二択（超えたら全部・超えなければ0）にしないことで、しきい値付近のちらつきを防ぐ
float3 Prefilter(float3 c)
{
    float br = max(c.r, max(c.g, c.b));
    float soft = clamp(br - g_Threshold + g_Knee, 0.0, 2.0 * g_Knee);
    soft = (soft * soft) / (4.0 * g_Knee + 1e-5);
    float contribution = max(soft, br - g_Threshold) / max(br, 1e-5);
    return c * contribution;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.TexCoord;
    float2 t = g_SrcTexelSize;

    // 13点の配置（読み込み元のテクセル単位）
    //   a . b . c
    //   . j . k .
    //   d . e . f
    //   . l . m .
    //   g . h . i
    float3 a = srcTexture.Sample(linearClamp, uv + t * float2(-2, -2)).rgb;
    float3 b = srcTexture.Sample(linearClamp, uv + t * float2(0, -2)).rgb;
    float3 c = srcTexture.Sample(linearClamp, uv + t * float2(2, -2)).rgb;
    float3 d = srcTexture.Sample(linearClamp, uv + t * float2(-2, 0)).rgb;
    float3 e = srcTexture.Sample(linearClamp, uv).rgb;
    float3 f = srcTexture.Sample(linearClamp, uv + t * float2(2, 0)).rgb;
    float3 g = srcTexture.Sample(linearClamp, uv + t * float2(-2, 2)).rgb;
    float3 h = srcTexture.Sample(linearClamp, uv + t * float2(0, 2)).rgb;
    float3 i = srcTexture.Sample(linearClamp, uv + t * float2(2, 2)).rgb;
    float3 j = srcTexture.Sample(linearClamp, uv + t * float2(-1, -1)).rgb;
    float3 k = srcTexture.Sample(linearClamp, uv + t * float2(1, -1)).rgb;
    float3 l = srcTexture.Sample(linearClamp, uv + t * float2(-1, 1)).rgb;
    float3 m = srcTexture.Sample(linearClamp, uv + t * float2(1, 1)).rgb;

    // 13点を「4点ずつの5グループ」として扱う
    //   外側の4つの四角（左上・右上・左下・右下）：各0.125
    //   中央の四角（j,k,l,m）：0.5
    float3 g0 = (a + b + d + e) * 0.25; // 左上
    float3 g1 = (b + c + e + f) * 0.25; // 右上
    float3 g2 = (d + e + g + h) * 0.25; // 左下
    float3 g3 = (e + f + h + i) * 0.25; // 右下
    float3 g4 = (j + k + l + m) * 0.25; // 中央

    float3 result;
    if (g_IsFirstPass > 0.5)
    {
        // Karis平均：グループごとに「明るいほど軽く」重み付けしてから正規化する
        float w0 = 0.125 * KarisWeight(g0);
        float w1 = 0.125 * KarisWeight(g1);
        float w2 = 0.125 * KarisWeight(g2);
        float w3 = 0.125 * KarisWeight(g3);
        float w4 = 0.5 * KarisWeight(g4);
        result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3 + g4 * w4) / (w0 + w1 + w2 + w3 + w4);

        // しきい値を超えた分だけを残す
        result = Prefilter(result);
    }
    else
    {
        // 2回目以降は普通の重み付き平均
        result = (g0 + g1 + g2 + g3) * 0.125 + g4 * 0.5;
    }

    // 負の値やNaNが混ざると連鎖全体に広がるので、念のため0以上に揃える
    result = max(result, 0.0);
    return float4(result, 1.0);
}
