// ---------------------------------------------------------
// 定数バッファ（C++側の ConstantBuffer 構造体と完全一致させる）
// ---------------------------------------------------------
cbuffer ConstantBuffer : register(b0)
{
    matrix mModel;
    matrix mView;
    matrix mProjection;
    float4 vLightDir; // ライトの方向 (wはダミー)
    float4 vLightColor; // ライトの色
};

// ---------------------------------------------------------
// シェーダーの入出力構造体
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
    float4 Pos : SV_POSITION; // システム用座標
    float3 Normal : NORMAL; // ワールド空間での法線
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
};

// テクスチャとサンプラーの設定
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// ---------------------------------------------------------
// 頂点シェーダー (VS)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // 座標変換 (Local -> World -> View -> Projection)
    float4 worldPos = mul(input.Pos, mModel);
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);
    
    // ★重要: 法線ベクトルをワールド空間に変換する（回転に対応させるため）
    // 本来はモデル行列の「逆転置行列」を掛けますが、等倍スケーリングならmModelのままでOK
    output.Normal = mul(float4(input.Normal, 0.0f), mModel).xyz;
    output.Normal = normalize(output.Normal); // 正規化
    
    // カラーとUVはそのままピクセルシェーダーに引き渡す
    output.Color = input.Color;
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // 1. テクスチャの色と頂点の色を乗算（LearnOpenGLでお馴染みの処理）
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color;
    
    // 2. Ambient (環境光) の計算
    float ambientStrength = 0.2f;
    float3 ambient = ambientStrength * vLightColor.xyz;
    
    // 3. Diffuse (拡散反射光) の計算
    // ライトの「進む方向」の逆向きベクトルを作る
    float3 lightDir = -vLightDir.xyz;
    
    // 法線とライト方向の内積を計算 (0.0以下は0.0にクランプ)
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * vLightColor.xyz;
    
    // 4. 最終的な色の結合
    float3 finalColor = (ambient + diffuse) * objectColor.xyz;
    
    return float4(finalColor, objectColor.a);
}