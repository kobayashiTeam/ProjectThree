
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

cbuffer PerSpriteBuffer : register(b3)
{
    float SpriteSize; // 板ポリの一辺の半分のサイズ
    float3 SpriteColor; // 色
};

// Vertex Shader出力と対応する入力形式
struct GSIn
{
    float4 WorldPos : TEXCOORD0;
};

struct GSOut
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

float4 ToClip(float4 worldPos)
{
    return mul(mul(worldPos, mView), mProjection);
}

// -----------------------------------------------
// Geometry Shader
// 1点 → 4頂点（TriangleStrip で四角形1枚）
//
// 頂点順（TriangleStripのため）:
//   0(左上) → 1(右上) → 2(左下) → 3(右下)
//   三角形① : 0,1,2
//   三角形② : 1,3,2
// -----------------------------------------------
[maxvertexcount(4)]
void GSmain(point GSIn input[1], inout TriangleStream<GSOut> stream)
{
    float3 center = input[0].WorldPos.xyz;
    float h = SpriteSize; // 半分のサイズ

    // XY平面固定の4隅（ワールド空間）
    // TriangleStripなので Z字順に並べる
    float3 corners[4] =
    {
        center + float3(-h, h, 0), // 0: 左上
        center + float3(h, h, 0), // 1: 右上
        center + float3(-h, -h, 0), // 2: 左下
        center + float3(h, -h, 0), // 3: 右下
    };

    float2 uvs[4] =
    {
        float2(0, 0), // 左上
        float2(1, 0), // 右上
        float2(0, 1), // 左下
        float2(1, 1), // 右下
    };

    for (int i = 0; i < 4; i++)
    {
        GSOut v;
        v.Pos = ToClip(float4(corners[i], 1.0f));
        v.UV = uvs[i];
        v.Color = float4(SpriteColor, 1.0f);
        stream.Append(v);
    }
    // TriangleStreamはRestartStrip不要（1枚の四角形として続く）
}
