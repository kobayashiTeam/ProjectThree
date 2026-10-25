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
    int g_radius; // 半径。(2r+1)×(2r+1) 個を平均する（CS版と同じ半径を使う）
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
    int radius = g_radius; // 大きくするほど強くぼける（1辺のサンプル数は常に奇数 2r+1）

    // ずらす量はちょうど整数ピクセルなので、Linear のサンプラーでも隣のピクセルの中心をそのまま読む
    // （補間は起きない。画面の外はサンプラーの CLAMP で端のピクセルになる。CS版の LoadClamped と同じ結果）
    for (int x = -radius; x <= radius; ++x)
    {
        for (int y = -radius; y <= radius; ++y)
        {
            float2 offset = float2(x, y) * g_texelSize;
            // 回数が定数バッファで変わるループの中なので、ミップの計算（微分）が要らない SampleLevel を使う
            sum += sceneTexture.SampleLevel(linearSampler, input.TexCoord + offset, 0);
        }
    }

    return sum / (float) ((radius * 2 + 1) * (radius * 2 + 1));
}
