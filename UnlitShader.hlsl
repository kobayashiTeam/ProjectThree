// ---------------------------------------------------------
// 定数バッファの分割
// ---------------------------------------------------------
// スロット0：フレーム単位で共通（カメラの情報のみ使用、ライト情報は無視）
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos; // Unlitでは未使用
    float4 vLightColor; // Unlitでは未使用
    float4 vEyePos; // Unlitでは未使用
    float4 vAttenuation; // Unlitでは未使用
};

// スロット1：オブジェクト単位（各モデルインスタンスの座標）
cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

// スロット2：マテリアル単位（将来、電球の色をマテリアルごとに変えたい場合用）
// cbuffer PerMaterialBuffer : register(b2) { };

// ---------------------------------------------------------
// 入出力構造体
// ---------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL; // Unlitでは未使用ですが頂点レイアウト維持のため残しています
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// ---------------------------------------------------------
// 頂点シェーダー (VS)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // 通常の座標変換
    float4 worldPos = mul(input.Pos, mModel);
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);
    
    output.Color = input.Color;
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // テクスチャと頂点カラーを掛け合わせる（白テクスチャなら純白になります）
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    
    // もし完全に「定数バッファに依存しない純白」にしたい場合は、
    // シンプルに return float4(1.0f, 1.0f, 1.0f, 1.0f); でもOKです。
    return texColor * input.Color;
}