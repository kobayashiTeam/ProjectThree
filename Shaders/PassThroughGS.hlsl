// Vertex Shaderの出力形式と一致させる
struct GSIn
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : POSITION;
};

typedef GSIn GSOut;

// 1つの三角形をそのまま出力
[maxvertexcount(3)]
void GSmain(
    triangle GSIn input[3],
    inout TriangleStream<GSOut> stream)
{
    stream.Append(input[0]);
    stream.Append(input[1]);
    stream.Append(input[2]);
}