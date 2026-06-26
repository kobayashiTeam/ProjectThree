// ShadowCubeVSPS.hlsl

cbuffer PerObjectCB : register(b1)
{
    matrix mModel;
};

cbuffer ShadowCubeCB : register(b3)
{
    float3 gLightPos;
    float gFarPlane;
};

// ---- VS ----
struct VS_IN
{
    float3 pos : POSITION;
};

struct VS_OUT
{
    float4 posW : TEXCOORD0; // ÉèÅ[ÉãÉhç¿ïWÇGSÇ…ìnÇ∑
};

VS_OUT VS(VS_IN vin)
{
    VS_OUT vout;
    vout.posW = mul(float4(vin.pos, 1.0f), mModel);
    return vout;
}

// ---- PS ----
struct GS_OUT
{
    float4 pos : SV_Position;
    float3 fragPosW : TEXCOORD0;
    uint slice : SV_RenderTargetArrayIndex;
};

float PS(GS_OUT pin) : SV_Depth
{
    float dist = length(pin.fragPosW - gLightPos);
    return dist / gFarPlane;
}