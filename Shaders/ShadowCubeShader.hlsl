cbuffer PerObjectCB : register(b1)
{
    matrix mModel;
};


cbuffer ShadowCubeCB : register(b4)
{
    matrix gLightViewProj[6]; // GSが使う
    float3 gLightPos;
    float gFarPlane;
};

// ---- VS ----
struct VS_INPUT
{
    float3 pos : POSITION;
};

struct VS_OUT
{
    float4 posW : TEXCOORD0; // ワールド座標をGSに渡す
};

VS_OUT VS(VS_INPUT input)
{
    VS_OUT vout;
    vout.posW = mul(float4(input.pos, 1.0f), mModel);
    return vout;
}

// ---- PS ----
struct PS_INPUT
{
    float4 pos : SV_Position;
    float3 fragPosW : TEXCOORD0;
    uint slice : SV_RenderTargetArrayIndex;
};

float PS(PS_INPUT input) : SV_Depth
{
    float dist = length(input.fragPosW - gLightPos);
    return dist / gFarPlane;
}