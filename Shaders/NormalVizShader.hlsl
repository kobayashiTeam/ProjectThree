cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};
cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

// VSÅ®GSÇ…ìnÇ∑ç\ë¢ëÃ
// SV_POSITIONÇÕGSì‡Ç≈ç¿ïWåvéZÇ…égÇ¶Ç»Ç¢ÇÃÇ≈WorldPosÇï ìrìnÇ∑
struct VS_OUTPUT
{
    float4 ClipPos : SV_POSITION;
    float3 Normal : NORMAL;
    float4 WorldPos : TEXCOORD0;
};

// GSÅ®PSÇ…ìnÇ∑ç\ë¢ëÃ
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

// ===================================================
// Vertex Shader
// ===================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;

    float4 worldPos = mul(input.Pos, mModel);
    output.WorldPos = worldPos;
    output.ClipPos = mul(mul(worldPos, mView), mProjection);
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);
    return output;
}

// ===================================================
// Pixel Shader
// ===================================================
float4 PS(PS_INPUT input) : SV_Target
{
    return input.Color;
}