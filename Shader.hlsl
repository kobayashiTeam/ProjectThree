// Shader.hlsl

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Color : COLOR; // ★C++のインプットレイアウトの"COLOR"と紐付く
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Color : COLOR; // ピクセルシェーダーに送る色
};

// 頂点シェーダー
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    output.Pos = input.Pos;
    output.Color = input.Color; // 色をそのままスルー
    return output;
}

// ピクセルシェーダー
float4 PS(PS_INPUT input) : SV_TARGET
{
    return float4(input.Color, 1.0f); // 補間された色にアルファ1.0を足して出力
}