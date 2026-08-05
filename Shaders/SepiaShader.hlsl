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
    float g_intensity; // 0.0 = 元の色, 1.0 = 完全なセピア
    float3 g_dummy;
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
    
    // セピア変換行列
    float3 sepia = float3(
        color.r * 0.393 + color.g * 0.769 + color.b * 0.189,
        color.r * 0.349 + color.g * 0.686 + color.b * 0.168,
        color.r * 0.272 + color.g * 0.534 + color.b * 0.131
    );
    
    float3 final = lerp(color.rgb, sepia, g_intensity);
    return float4(final, color.a);
}