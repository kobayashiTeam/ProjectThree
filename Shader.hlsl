// --- Shader.hlsl ---

cbuffer ConstantBuffer : register(b0)
{
    float offsetX;
    float3 dummy;
};

// ★追加: テクスチャとサンプラーの定義
Texture2D txDiffuse : register(t0); // t0スロットのテクスチャ
SamplerState samLinear : register(s0); // s0スロットのサンプラー

// 頂点シェーダーへの入力構造体
struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Color : COLOR;
    float2 Tex : TEXCOORD0; // ★追加：UV座標
};

// ピクセルシェーダーへの入力構造体（VSからの出力）
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Color : COLOR;
    float2 Tex : TEXCOORD0; // ★追加：UV座標をピクセルシェーダーに引き渡す
};

// 頂点シェーダー
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // アニメーション用のX軸移動を適用
    float4 pos = float4(input.Pos, 1.0f);
    pos.x += offsetX;
    
    output.Pos = pos;
    output.Color = input.Color;
    output.Tex = input.Tex; // ★UV座標をそのままラスタライザへ渡す（自動で補間されます）
    
    return output;
}

// ピクセルシェーダー
float4 PS(PS_INPUT input) : SV_Target
{
    // ★LearnOpenGLの texture(texture1, TexCoords) に相当する処理
    // テクスチャから色を抽出し、頂点カラー（今回は白）を掛け合わせる
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    
    return texColor * float4(input.Color, 1.0f);
}