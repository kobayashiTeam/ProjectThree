// 1. 定数バッファの定義（C++側とサイズ・並びを完全に一致させる）
cbuffer ConstantBuffer : register(b0)
{
    matrix Model; // 4x4行列 (float4x4)
    matrix View; // 4x4行列
    matrix Projection; // 4x4行列
};

// 頂点シェーダーへの入力構造体
struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

// 頂点シェーダーからの出力（ピクセルシェーダーへの入力）構造体
struct PS_INPUT
{
    float4 Pos : SV_POSITION; // システム用セマンティクス（gl_Position相当）
    float3 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

// テクスチャとサンプラーのバインド
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// ------------------------------------------------------------------------
// 頂点シェーダー
// ------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // 頂点座標をローカル空間からクリップ空間へ変換
    // ※行優先行列の場合、mul(ベクトル, 行列) の順番で掛け算します
    float4 pos = float4(input.Pos, 1.0f);
    pos = mul(pos, Model);
    pos = mul(pos, View);
    pos = mul(pos, Projection);
    
    output.Pos = pos;
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    
    return output;
}

// ------------------------------------------------------------------------
// ピクセルシェーダー
// ------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // テクスチャの色をサンプリングし、頂点カラー（今回は白）を乗算
    return txDiffuse.Sample(samLinear, input.TexCoord) * float4(input.Color, 1.0f);
}