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
    float g_blurAmount; // 0.0 = 無し, 1.0～ で強くなる
    float2 g_texelSize; // (1.0/width, 1.0/height) をC++側から渡す
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
    float4 sum = 0.0f;
    int radius = 2; // ぼかし範囲。大きくするほど強くぼける（サンプル数は常に奇数になる設計）

    for (int x = -radius; x <= radius; ++x)
    {
        for (int y = -radius; y <= radius; ++y)
        {
            float2 offset = float2(x, y) * g_texelSize * g_blurAmount;
            sum += sceneTexture.Sample(linearSampler, input.TexCoord + offset);
        }
    }
    
    return sum / ((radius * 2 + 1) * (radius * 2 + 1));
}