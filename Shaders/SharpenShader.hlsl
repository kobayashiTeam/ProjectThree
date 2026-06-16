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
    float g_sharpness; // 0.0～1.0（0.5くらいが自然）
    float2 g_texelSize;
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
    float4 center = sceneTexture.Sample(linearSampler, input.TexCoord);
    
    float4 left = sceneTexture.Sample(linearSampler, input.TexCoord + float2(-g_texelSize.x, 0));
    float4 right = sceneTexture.Sample(linearSampler, input.TexCoord + float2(g_texelSize.x, 0));
    float4 up = sceneTexture.Sample(linearSampler, input.TexCoord + float2(0, -g_texelSize.y));
    float4 down = sceneTexture.Sample(linearSampler, input.TexCoord + float2(0, g_texelSize.y));
    
    float4 sharpened = center * (1.0f + 4.0f * g_sharpness)
                     - (left + right + up + down) * g_sharpness;
    
    return float4(sharpened.rgb, center.a);
}