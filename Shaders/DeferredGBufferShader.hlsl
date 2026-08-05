// ---------------------------------------------------------
// 定数バッファ（既存のものをそのまま維持）
// ---------------------------------------------------------
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor;
};

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
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : TEXCOORD1; // ワールド座標をPSに渡す
};

// --- 【遅延用】3枚のG-Buffer（MRT）への出力定義 ---
struct PS_OUTPUT
{
    float4 Color : SV_Target0; // RT0: アルベド（基本色）
    float4 Normal : SV_Target1; // RT1: 法線（向き）
    float4 Position : SV_Target2; // RT2: ワールド座標（位置）
};

// テクスチャ・サンプラー
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// ---------------------------------------------------------
// 頂点シェーダー (VS)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;

    // ワールド座標の計算
    float4 worldPos = mul(input.Pos, mModel);
    output.WorldPos = worldPos.xyz;

    // スクリーン座標への変換
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);

    // 法線の計算（モデルの回転・拡大縮小を反映）
    output.Normal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);

    // 色とUVはそのままPSへフォワード
    output.Color = input.Color;
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
PS_OUTPUT PS(PS_INPUT input)
{
    PS_OUTPUT output;

    float4 testColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    // 1. 【基本色の抽出】
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color * testColor;//vMaterialColor
    
    // RT0 にマテリアル本来の色をそのまま書き込む
    output.Color = objectColor;

    // 2. 【法線情報の書き込み】
    float3 normal = normalize(input.Normal);
    
    // 法線を0～1範囲へ変換して保存
    float3 packedNormal = normal * 0.5f + 0.5f;
    
    // RT1 に法線を書き込む（W要素は将来のマテリアルID用に1.0を割り振っておきます）
    output.Normal = float4(packedNormal, 1.0f);

    // 3. 【ワールド座標の書き込み】
    // RT2 にピクセルの3次元位置情報をそのまま書き込む
    // ※RT2のフォーマットはC++側で「DXGI_FORMAT_R32G32B32A32_FLOAT」などの高精度な浮動小数点テクスチャにしてください
    output.Position = float4(input.WorldPos, 1.0f);

    return output;
}