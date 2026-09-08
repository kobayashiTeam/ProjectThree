// GBufferDebugAlphaBlit.hlsl
// Scene9（PBRマテリアルグリッド）用：G-Bufferのalphaチャンネル（metallic/roughness）を
// グレースケールとしてそのままバックバッファへ映すだけの単純なシェーダー。
// GBufferDebugBlit.hlsl（rgbを表示）の兄弟版で、aを表示する点だけが違う。

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

Texture2D sourceTexture : register(t0);
SamplerState linearSampler : register(s0);

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    // aチャンネル（Albedoならmetallic、Normalならroughness）をそのままグレースケールとして出力
    float value = sourceTexture.Sample(linearSampler, input.TexCoord).a;
    return float4(value, value, value, 1.0);
}
