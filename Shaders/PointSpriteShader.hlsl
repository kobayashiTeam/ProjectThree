// PointSprite_VSPS.hlsl
// 点群からGSで板ポリを生成するためのVS/PS

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

// -----------------------------------------------
// 入力: 点1つ（位置だけでOK）
// -----------------------------------------------
struct VS_INPUT
{
    float3 Pos : POSITION;
};

// -----------------------------------------------
// VS→GS: ワールド座標をそのまま渡す
// SV_POSITIONはGSで計算するのでここでは出さない
// -----------------------------------------------
struct VS_OUTPUT
{
    float4 WorldPos : TEXCOORD0;
};

// -----------------------------------------------
// Vertex Shader
// 点のワールド座標を計算してGSへ渡すだけ
// -----------------------------------------------
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.WorldPos = mul(float4(input.Pos, 1.0f), mModel);
    return output;
}

// -----------------------------------------------
// GS→PS
// -----------------------------------------------
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0; // 板ポリ上のUV（将来テクスチャを貼るときに便利）
    float4 Color : COLOR;
};

// -----------------------------------------------
// Pixel Shader
// -----------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    return input.Color;
}
