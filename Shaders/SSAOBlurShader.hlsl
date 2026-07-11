// =========================================================
// SSAO (Screen Space Ambient Occlusion) ブラーシェーダー
// 生のSSAOマップのノイズを、エッジを維持しながら滑らかにします。
// =========================================================

// ---------------------------------------------------------
// 入出力構造体
// ---------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

// ---------------------------------------------------------
// リソース (テクスチャとサンプラー)
// ---------------------------------------------------------
Texture2D txInputSSAO : register(t0); // 前パスで生成した生のSSAOマップ (R8_UNORM等)
SamplerState samLinearClamp : register(s0); // ぼかし処理のためLinearサンプラーを使用

// ---------------------------------------------------------
// 頂点シェーダー (フルスクリーンクアッド用)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // 入力された頂点座標をそのまま画面の座標(Clip Space)として扱う
    output.Pos = float4(input.Pos.x, input.Pos.y, 0.0f, 1.0f);
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー (4x4エッジ維持ブラー)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    float2 texelSize;
    // テクスチャの解像度（横幅、縦幅）を取得して、1ピクセルあたりのUV移動量を計算する
    txInputSSAO.GetDimensions(texelSize.x, texelSize.y);
    float2 texel = 1.0f / texelSize;
    
    float result = 0.0f;
    
    // 注目しているピ心を中心とした -2 ～ 1 の 4x4 の範囲を調べる
    // (4x4ノイズテクスチャによるザラザラ感を綺麗に打ち消すため)
    [unroll]
    for (int x = -2; x < 2; ++x)
    {
        [unroll]
        for (int y = -2; y < 2; ++y)
        {
            // 周辺ピクセルのUV座標を計算
            float2 offset = float2(float(x), float(y)) * texel;
            
            // 周辺ピクセルのSSAO値をサンプリングして加算
            result += txInputSSAO.Sample(samLinearClamp, input.Tex + offset).r;
        }
    }
    
    // 16個のサンプルの平均値を算出
    float finalOcclusion = result / 16.0f;
    
    // 最終的な滑らかなSSAO値をRGBすべてに出力
    return float4(finalOcclusion, finalOcclusion, finalOcclusion, 1.0f);
}