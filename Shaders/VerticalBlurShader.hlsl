// ---------------------------------------------------------
// 入出力構造体（共通）
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
Texture2D txHorizontalBlurred : register(t0);
SamplerState samLinear : register(s0);

// ガウシアンブラーの重み係数（1次元カーネル）
static const float weight[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

// ---------------------------------------------------------
// ピクセルシェーダー（PS）メイン
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // テクスチャの現在のサイズから1ピクセルあたりのUV高さを計算
    float width, height;
    txHorizontalBlurred.GetDimensions(width, height);
    float tex_offset = 1.0f / height;

    // 現在のピクセル（中央）
    float3 result = txHorizontalBlurred.Sample(samLinear, input.Tex).rgb * weight[0];

    // 上下に4ピクセルずつサンプリングを広げて足し合わせる
    for (int i = 1; i < 5; ++i)
    {
        result += txHorizontalBlurred.Sample(samLinear, input.Tex + float2(0.0f, tex_offset * i)).rgb * weight[i];
        result += txHorizontalBlurred.Sample(samLinear, input.Tex - float2(0.0f, tex_offset * i)).rgb * weight[i];
    }

    return float4(result, 1.0f);
}