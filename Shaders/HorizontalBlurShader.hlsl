// ---------------------------------------------------------
// 入出力構造体
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
Texture2D txBright : register(t0);
SamplerState samLinear : register(s0);

// ガウシアンブラーの重み係数（1次元カーネル）
static const float weight[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

// ---------------------------------------------------------
// ピクセルシェーダー（PS）メイン
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // テクスチャの現在のサイズから1ピクセルあたりのUV幅を計算
    float width, height;
    txBright.GetDimensions(width, height);
    float tex_offset = 1.0f / width;

    // 現在のピクセル（中央）
    float3 result = txBright.Sample(samLinear, input.Tex).rgb * weight[0];

    // 左右に4ピクセルずつサンプリングを広げて足し合わせる
    for (int i = 1; i < 5; ++i)
    {
        result += txBright.Sample(samLinear, input.Tex + float2(tex_offset * i, 0.0f)).rgb * weight[i];
        result += txBright.Sample(samLinear, input.Tex - float2(tex_offset * i, 0.0f)).rgb * weight[i];
    }

    return float4(result, 1.0f);
}