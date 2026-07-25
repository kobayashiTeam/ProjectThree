cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
}

cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
}

cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor;
}

//入出力構造体
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_Position;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

//頂点シェーダ
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    //position
    float4 worldPos = mul(input.Pos, mModel);
    output.Pos = mul(input.Pos, mModel); //worldPos.xyz;
    output.Pos = mul(output.Pos, mView);
    output.Pos = mul(output.Pos, mProjection);
    //normal
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);
    //color
    output.Color = input.Color;
    //tex
    output.Tex = input.Tex;
    
    return output;
}

float4 PS(PS_INPUT input):SV_Target
{
    return input.Color;
}