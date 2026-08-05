// Geometry Shader
// 生成された三角形全体を指定量だけ移動する
struct GSIn
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : POSITION;
};

typedef GSIn GSOut;

cbuffer CBMove : register(b3)
{
    float offsetX;
    float offsetY;
    float offsetZ;
    float _pad;
};

[maxvertexcount(3)]
void GSmain(
    triangle GSIn input[3],
    inout TriangleStream<GSOut> stream)
{
    for (int i = 0; i < 3; i++)
    {
        GSOut output = input[i];

        
        output.Pos.x += offsetX;
        output.Pos.y += offsetY;
        output.Pos.z += offsetZ;

        stream.Append(output);
    }
}