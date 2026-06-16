// ---------------------------------------------------------
// 定数バッファ（C++側と完全一致させる）
// ---------------------------------------------------------
// ★変更が必要
cbuffer ConstantBuffer : register(b0)
{
    matrix mModel;
    matrix mView;
    matrix mProjection;
    float4 vLightPos; // vLightDir → vLightPos に変更
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation; // ★追加（C++側に合わせる）
};

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
    float3 WorldPos : POSITION; // ★追加：ワールド空間でのピクセルの位置
};

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// ---------------------------------------------------------
// 頂点シェーダー (VS)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // ワールド座標を計算してピクセルシェーダーに渡す
    float4 worldPos = mul(input.Pos, mModel);
    output.WorldPos = worldPos.xyz; // ★格納
    
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);
    
    output.Normal = mul(float4(input.Normal, 0.0f), mModel).xyz;
    output.Normal = normalize(output.Normal);
    
    output.Color = input.Color;
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // ★ vLightColor.w == 0 なら「光源オブジェクト」として白を返す
    if (vLightColor.w == 0.0f)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f); // 純白で描画（電球自体は減衰しない）
    }
    
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color;
    
    // --------------------------------------------------------
    // 【新規計算】光源とピクセル間の距離と減衰率の計算
    // --------------------------------------------------------
    // 光源からピクセルへのベクトルを一度計算（正規化前）
    float3 lightVec = vLightPos.xyz - input.WorldPos;
    
    // length関数で「距離 (d)」を計算
    float distance = length(lightVec);
    
    // ライトの方向ベクトル（正規化）
    float3 lightDir = normalize(lightVec);
    
    // 減衰率の計算 (C++側から送られた vAttenuation.xyz を使用)
    // x: Constant(1.0), y: Linear(0.09), z: Quadratic(0.032)
    float attenuation = 1.0f / (vAttenuation.x +
                                 vAttenuation.y * distance +
                                 vAttenuation.z * (distance * distance));
    
    // --------------------------------------------------------
    // 各ライティング成分の計算（既存のロジック）
    // --------------------------------------------------------
    // 1. Ambient (環境光)
    float ambientStrength = 0.2f;
    float3 ambient = ambientStrength * vLightColor.xyz;
    
    // 2. Diffuse (拡散反射光)
    float3 normal = normalize(input.Normal);
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diff * vLightColor.xyz;
    
    // 3. Specular (鏡面反射光：Blinn-Phongモデル)
    float specularStrength = 0.5f;
    float3 viewDir = normalize(vEyePos.xyz - input.WorldPos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    
    float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
    float3 specular = specularStrength * spec * vLightColor.xyz;
    
    // --------------------------------------------------------
    // 【変更】すべての光の成分に減衰率（attenuation）を乗算する
    // --------------------------------------------------------
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    // 4. 最終的な色の結合
    float3 finalColor = (ambient + diffuse) * objectColor.xyz + specular;
    
    return float4(finalColor, objectColor.a);
}