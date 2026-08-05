// Scene3のDepth表示専用。GBufferDebugBlit.hlslとの違いは1点だけ：
// 生の非線形depth値をそのまま出すと近距離?中距離がほぼ全部1.0付近に張り付いて
// 真っ赤（濃淡なし）に見えてしまうため、ここで線形化してからグレースケール化する。

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = float4(input.Position, 1.0);
    output.TexCoord = input.TexCoord;
    return output;
}

Texture2D sourceTexture : register(t0);
SamplerState linearSampler : register(s0);

// camera.cppのXMMatrixPerspectiveFovLHに渡しているnear/farと一致させること
static const float NEAR_Z = 0.01f;
static const float FAR_Z = 1000.0f;
// 見やすさ調整用の目安値（シーンのスケールに応じて変えてよい）
static const float VISUALIZE_RANGE = 15.0f;

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float rawDepth = sourceTexture.Sample(linearSampler, input.TexCoord).r;

    // 非線形depth(0~1) → カメラからの実距離（view空間Z）へ変換
    float linearZ = (FAR_Z * NEAR_Z) / (FAR_Z - rawDepth * (FAR_Z - NEAR_Z));

    // 0~VISUALIZE_RANGEの範囲を0~1のグレースケールに正規化
    float gray = 1-saturate(linearZ / VISUALIZE_RANGE);

    return float4(gray, gray, gray, 1.0);
}
