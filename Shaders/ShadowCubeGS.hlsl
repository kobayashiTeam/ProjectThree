// ShadowCubeGS.hlsl

cbuffer ShadowCubeCB : register(b3)
{
    matrix gLightViewProj[6];
    float3 gLightPos;
    float gFarPlane;
};

struct GSIn
{
    float4 posW : TEXCOORD0;
};

struct GSOut
{
    float4 pos : SV_Position;
    float3 fragPosW : TEXCOORD0;
    uint slice : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void GS(triangle GSIn input[3], inout TriangleStream<GSOut> stream)
{
    for (int face = 0; face < 6; face++)
    {
        for (int v = 0; v < 3; v++)
        {
            GSOut gout;
            gout.pos = mul(gin[v].posW, gLightViewProj[face]);
            gout.fragPosW = gin[v].posW.xyz;
            gout.slice = face;
            stream.Append(gout);
        }
        stream.RestartStrip();
    }
}