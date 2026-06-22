// NormalVis_VSPS.hlsl
// 既存のVS/PSと同じcbuffer構成をそのまま流用

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

// VS→GSに渡す構造体
// SV_POSITIONはGS内で座標計算に使えないのでWorldPosを別途渡す
struct VS_OUTPUT
{
    float4 ClipPos : SV_POSITION;
    float3 Normal : NORMAL;
    //float4 WorldPos : POSITION; // GS内でオフセット計算に使う
    float4 WorldPos : TEXCOORD0;
};

// GS→PSに渡す構造体
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
    output.WorldPos = worldPos; //worldPos//float4(1,0,0,1)
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
    return float4(1, 0, 0, 1);

}