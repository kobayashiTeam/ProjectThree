// NormalVis_GS.hlsl

cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

cbuffer CBNormalVis : register(b3)
{
    float3 NormalColor; // 12バイト
    float NormalLength; // 4バイト (これで綺麗に16バイト)
};

// VSOutputと完全一致させること（セマンティクスも含めて）
struct GSIn
{
    float4 ClipPos : SV_POSITION;
    float3 Normal : NORMAL;
    //float4 WorldPos : POSITION;
    float4 WorldPos : TEXCOORD0;
};

struct GSOut
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

float4 ToClip(float4 worldPos)
{
    return mul(mul(worldPos, mView), mProjection);
}


//新しい方
//[maxvertexcount(6)]
//void GSmain(
//    triangle GSIn input[3],
//    inout LineStream<GSOut> stream)
//{
//    for (int i = 0; i < 3; i++)
//    {
//        GSOut p0;
//        p0.Pos = input[i].ClipPos;
//        p0.Color = float4(1, 0, 0, 1);
//        stream.Append(p0);

//        GSOut p1;
//        p1.Pos = input[i].ClipPos;
//        p1.Pos.y += 0.4f * p1.Pos.w; // ←重要
//        p1.Color = float4(0, 1, 0, 1);//赤から緑
//        stream.Append(p1);

//        stream.RestartStrip();
//    }
//}



[maxvertexcount(6)] // 三角形の3頂点 × 2頂点(線分) = 6
void GSmain(triangle GSIn input[3], inout LineStream<GSOut> lineStream)
{
    float normalLength = 5.0f; // 法線ラインの長さ（ワールド単位）

    for (int i = 0; i < 3; i++)
    {
        float3 base = input[i].WorldPos.xyz;
        float3 tip = base + normalize(input[i].Normal) * normalLength;
        //float3 tip = base + float3(0,1,0) * normalLength;

        GSOut v0, v1;

        // 根元（白）
        v0.Pos = ToClip(float4(base, 1.0f));
        v0.Color = float4(1.0f, 1.0f, 0.0f, 1.0f); // 黄色

        // 先端（緑）
        v1.Pos = ToClip(float4(tip, 1.0f));
        v1.Color = float4(0.0f, 1.0f, 0.0f, 1.0f); // 緑

        lineStream.Append(v0);
        lineStream.Append(v1);
        lineStream.RestartStrip(); // 線分ごとにリスタート
    }
}
