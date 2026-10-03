// LDRのポストプロセスまで終わった絵に、最後にガンマ補正だけを掛けてバックバッファへ出力する
// （トーンマッピングは Tonemap.hlsl へ移動。HDR→LDRの変換はそちらで済んでいる）
cbuffer PostProcessConfig : register(b5)
{
    float g_Exposure; // Tonemap用（ここでは未使用）
    float g_GammaCorrection; // Scene2用：1.0=ON（ガンマ補正あり）、0.0=OFF（補正なし・リニアのまま出力）
    float g_Tonemapper; // Tonemap用（ここでは未使用）
    float g_BloomIntensity; // Tonemap用（ここでは未使用）
};

struct VS_INPUT
{
    float3 Position : POSITION; // NDC座標 (-1.0 ~ 1.0)
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

Texture2D sceneTexture : register(t0); // LDR（0〜1、リニア）の最終結果
SamplerState linearSampler : register(s0);


float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);
    float3 rgb = color.rgb;

    // ガンマ補正（モニター表示用の色空間へ変換）
    // Scene2用：g_GammaCorrectionがOFFのときはあえて補正をかけず、リニアのまま出力する
    if (g_GammaCorrection > 0.5)
    {
        rgb = pow(saturate(rgb), 1.0 / 2.2);
    }

    return float4(rgb, color.a);
}
