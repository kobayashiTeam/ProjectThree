// 頂点シェーダーに入ってくるデータの形
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Color : COLOR; // ★C++のインプットレイアウトの"COLOR"と紐付く
};

// 頂点シェーダーから出て、ピクセルシェーダーに入っていくデータの形
struct PS_INPUT
{
    float4 Pos : SV_POSITION; // システム用（画面座標）
    float3 Color : COLOR; // ピクセルシェーダーに渡す色
};

// 頂点シェーダー
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    output.Pos = input.Pos;
    output.Color = input.Color; // そのままピクセルシェーダーへスルー
    return output;
}

// ピクセルシェーダー
float4 PS(PS_INPUT input) : SV_TARGET
{
    // 頂点の間はGPUが自動で綺麗に補間（グラデーション）してくれます
    return float4(input.Color, 1.0f); // アルファ値 1.0 を足して出力
}