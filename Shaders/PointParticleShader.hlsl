// =============================================
// PointParticle.hlsl
// =============================================

cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos; // カメラ位置（wは無視）
    float4 vAttenuation;
};

// ======================
// Vertex Shader
// ======================
struct VS_INPUT
{
    float3 Position : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    // float PointSize : SV_PointSize;   // 後で有効化
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;

    // 正しい順番：Projection * View * Position
    float4 worldPos = float4(input.Position, 1.0f);
    float4 viewPos = mul(mView, worldPos);
    output.Position = mul(mProjection, viewPos);

    // 距離計算（PointSize用） - 将来ここで使う
    float dist = length(vEyePos.xyz - input.Position); // .xyzを明示的に取る方が安全

    // output.PointSize = 8.0f / max(dist * 0.08f, 0.5f);   // 後で有効化

    return output;
}

// ======================
// Pixel Shader
// ======================
float4 PS(VS_OUTPUT input) : SV_Target
{
    return float4(1.0f, 0.65f, 0.2f, 1.0f); // 明るいオレンジ
}