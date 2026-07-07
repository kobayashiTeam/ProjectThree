// ---------------------------------------------------------
// 入出力構造体（共通レイアウトを維持）
// ---------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

// ---------------------------------------------------------
// 共通頂点シェーダー（VS）
// ポストプロセス用のフルスクリーンQuadをそのままパススルーします
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    output.Pos = input.Pos;
    output.Tex = input.Tex;
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー（PS）側リソース
// ---------------------------------------------------------
Texture2D txScene : register(t0); // 通常のカラー画面（ピンポンチェーンの直前の結果）
Texture2D txBlur : register(t1); // 事前処理で完全にボケ上がった高輝度テクスチャ

SamplerState samLinear : register(s0);

// ---------------------------------------------------------
// ピクセルシェーダー（PS）メイン
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // 1. 通常のシーンカラーをサンプリング
    float4 sceneColor = txScene.Sample(samLinear, input.Tex);
    
    // 2. 事前に作っておいたボケ（光の溢れ）成分をサンプリング
    float3 blurColor = txBlur.Sample(samLinear, input.Tex).rgb;

    // 3. 通常シーンにボケ足を「加算合成（足し算）」する
    // ※元の画面のアルファ値（透過度情報など）を壊さないよう、rgbだけ加算してaはそのまま残します
    float3 finalColor = sceneColor.rgb + blurColor;

    return float4(finalColor, sceneColor.a);
}