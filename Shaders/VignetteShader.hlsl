// ==========================================
// 1. 入出力構造体の定義
// ==========================================
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

// ==========================================
// 2. 定数バッファとリソースのバインド
// ==========================================
// スロット0：これまでの描画結果（テクスチャ）
Texture2D sceneTexture : register(t0);
SamplerState linearSampler : register(s0);

// スロット2：C++側の PerEffectCB と完全に一致させる
cbuffer PerEffectCB : register(b2)
{
    float g_intensity; // 0.0 = ビネット無し, 1.0 = 最大限に暗くなる
    float g_radius; // 0.5～1.0くらい
    float g_softness; // 0.1～0.5
    float g_dummy;
};

// ==========================================
// 3. 頂点シェーダー (VS)
// ==========================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    // NDC空間の座標をそのまま出力（行列変換なし）
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}

// ==========================================
// 4. ピクセルシェーダー (PS)
// ==========================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);
    
    float2 center = float2(0.5f, 0.5f);
    float dist = distance(input.TexCoord, center);
    
    float vignette = smoothstep(g_radius, g_radius - g_softness, dist);
    float3 final = color.rgb * (1.0f - (1.0f - vignette) * g_intensity);
    
    return float4(final, color.a);
}