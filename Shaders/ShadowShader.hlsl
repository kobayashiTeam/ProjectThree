// Shadow.hlsl（新規作成）
cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
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