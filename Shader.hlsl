// Shader.hlsl

// ★ C++から送られてくる定数バッファ（0番スロットに紐付く）
cbuffer MyConstantBuffer : register(b0)
{
    float offsetX;
    // シェーダー側では、パディング（dummy）は自動で解釈されるため書かなくても大丈夫です
};

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Color : COLOR;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Color : COLOR;
};

// 頂点シェーダー
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // ★ C++から送られてきた offsetX をX座標に足し算して動かす！
    output.Pos = input.Pos;
    output.Pos.x += offsetX;
    
    output.Color = input.Color;
    return output;
}

// （ピクセルシェーダー PS は前回のままでOKです）

// ピクセルシェーダー
float4 PS(PS_INPUT input) : SV_TARGET
{
    return float4(input.Color, 1.0f); // 補間された色にアルファ1.0を足して出力
}