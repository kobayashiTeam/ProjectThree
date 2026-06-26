// ShadowCubeGS.hlsl

cbuffer ShadowCubeCB : register(b4)
{
    matrix gLightViewProj[6]; // GS‚ªg‚¤
    float3 gLightPos;
    float gFarPlane;
};


struct GSIn
{
    float4 posW : TEXCOORD0;//texcoord‚Å‚ ‚é‚±‚Æ‚ÉˆÓ–¡‚Í‚È‚¢A‰½‚Ì©“®ˆ—‚à‚¹‚¸‚Éfloat4‚Å‰^‚Ôw¦
};

struct GSOut
{
    float4 pos : SV_Position;
    float3 fragPosW : TEXCOORD0;
    uint slice : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void GSmain(triangle GSIn input[3], inout TriangleStream<GSOut> stream)
{
    for (int face = 0; face < 6; face++)
    {
        for (int v = 0; v < 3; v++)
        {
            GSOut gout;
            gout.pos = mul(input[v].posW, gLightViewProj[face]);
            gout.fragPosW = input[v].posW.xyz;
            gout.slice = face;
            stream.Append(gout);
        }
        stream.RestartStrip();
    }
}