// Shadow.hlsl（新規作成）
cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

// LightDataの1ライト分
struct LightData
{
    float4 position;
    float4 direction;
    float4 color;
    matrix lightSpaceMatrix;
    int type;
    float intensity;
    float farPlane;
    float padding;
};

cbuffer LightBuffer : register(b3)
{
    // LightDataの定義...
    LightData lights[4];
    int lightCount;
    float3 padding;
};

float4 VS(float4 pos : POSITION) : SV_POSITION
{
    float4 worldPos = mul(pos, mModel);
    float4 lightViewPos = mul(worldPos, lights[0].lightSpaceMatrix);
    return lightViewPos;
}
// PSはなし
// 空のPS（コンパイルエラー回避用）
float4 PS() : SV_Target
{
    return float4(0, 0, 0, 0);
}