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
    float g_intensity; // モノクロの強さ (0.0 = 通常, 1.0 = 完全なモノクロ)
    float3 g_dummy; // 16バイトアライメント用のパディング（HLSL側でも一応受けておく）
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
    // ① 元の画面の色をサンプリング
    float4 origColor = sceneTexture.Sample(linearSampler, input.TexCoord);
    
    // ② 輝度（Luminance）を計算してモノクロの色を作る
    // 人間の目の感度（緑を強く感じ、青を弱く感じる）に合わせた標準的な重み付けです
    float gray = dot(origColor.rgb, float3(0.299f, 0.587f, 0.114f));
    float3 grayColor = float3(gray, gray, gray);
    
    // ③ lerp（線形補間）関数を使って、intensity の割合で元の色とモノクロ色を混ぜる
    // g_intensity = 0.0f のとき -> origColor.rgb
    // g_intensity = 1.0f のとき -> grayColor
    float3 finalColor = lerp(origColor.rgb, grayColor, g_intensity);
    
    // ④ アルファ値（透明度）は元の画面のまま返す
    return float4(finalColor, origColor.a);
}