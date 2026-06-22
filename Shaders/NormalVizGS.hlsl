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
    float4 WorldPos : POSITION;
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

//[maxvertexcount(6)]
//void GSmain(
//    triangle GSIn input[3],
//    inout LineStream<GSOut> stream)
//{
//    for (int i = 0; i < 3; i++)
//    {
//        float3 origin = input[i].WorldPos.xyz;
//        float3 tip = origin + input[i].Normal * NormalLength;

//        // 根元
//        GSOut p0;
//        p0.Pos = ToClip(float4(origin, 1.0f));
//        p0.Color = float4(NormalColor, 1.0f);
//        stream.Append(p0);

//        // 先端
//        GSOut p1;
//        p1.Pos = ToClip(float4(tip, 1.0f));
//        p1.Color = float4(NormalColor, 1.0f);
//        stream.Append(p1);

//        stream.RestartStrip(); // 線分を切断（繋げない）
//    }
//}

[maxvertexcount(6)]
void GSmain(
    triangle GSIn input[3],
    inout LineStream<GSOut> stream)
{
    for (int i = 0; i < 3; i++)
    {
        float3 origin = input[i].WorldPos.xyz;

        // 法線を使わない
        float3 tip = origin + float3(0, 1, 0);

        GSOut p0;
        p0.Pos = ToClip(float4(origin, 1));
        p0.Color = float4(1, 0, 0, 1);
        stream.Append(p0);

        GSOut p1;
        p1.Pos = ToClip(float4(tip, 1));
        p1.Color = float4(1, 0, 0, 1);
        stream.Append(p1);

        stream.RestartStrip();
    }
}

