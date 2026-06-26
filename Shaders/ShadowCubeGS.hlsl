// ShadowCubeGS.hlsl

cbuffer ShadowCubeCB : register(b3)
{
    matrix gLightViewProj[6];
    float3 gLightPos;
    float gFarPlane;
};

struct VS_OUT
{
    float4 posW : TEXCOORD0;
};

struct GS_OUT
{
    float4 pos : SV_Position;
    float3 fragPosW : TEXCOORD0;
    uint slice : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void GS(triangle VS_OUT gin[3], inout TriangleStream<GS_OUT> stream)
{
    for (int face = 0; face < 6; face++)
    {
        for (int v = 0; v < 3; v++)
        {
            GS_OUT gout;
            gout.pos = mul(gin[v].posW, gLightViewProj[face]);
            gout.fragPosW = gin[v].posW.xyz;
            gout.slice = face;
            stream.Append(gout);
        }
        stream.RestartStrip();
    }
}