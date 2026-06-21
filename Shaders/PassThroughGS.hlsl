// PassThroughGS.hlsl

// LitShader.hlslのPS_INPUTと完全一致させる
struct GSIn
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : POSITION;
};

typedef GSIn GSOut;

[maxvertexcount(3)]//1下位の呼び出しで出力する頂点の最大数。３つずつ
void GSmain(
    triangle GSIn input[3],
    inout TriangleStream<GSOut> stream)
{
    stream.Append(input[0]);
    stream.Append(input[1]);
    stream.Append(input[2]);
}