// ---------------------------------------------------------
// 定数バッファの分割
// ---------------------------------------------------------
// スロット0：フレーム単位で共通（カメラやライトの情報）
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

// スロット1：オブジェクト単位（各モデルインスタンスの座標）
cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor; // C++側の構造体と完全一致させる
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
    float3 WorldPos : POSITION; // ワールド空間でのピクセルの位置
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
    output.WorldPos = worldPos.xyz;
    
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);
    
    // 法線ベクトルのワールド変換
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
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    
    // ★オブジェクトの色に、マテリアル固有の色（vMaterialColor）も掛け合わせる！
    float4 objectColor = texColor * input.Color * vMaterialColor;
    
    // 光源からピクセルへのベクトル（正規化前）
    float3 lightVec = vLightPos.xyz - input.WorldPos;
    
    // 距離 (d) の計算
    float distance = length(lightVec);
    
    // ライトの方向ベクトル（正規化）
    float3 lightDir = normalize(lightVec);
    
    // 減衰率の計算
    float attenuation = 1.0f / (vAttenuation.x +
                                 vAttenuation.y * distance +
                                 vAttenuation.z * (distance * distance));
    
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
    
    // すべての光の成分に減衰率を乗算
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    
    // 最終的な色の結合
    float3 finalColor = (ambient + diffuse) * objectColor.xyz + specular;
    
    return float4(finalColor, objectColor.a);
}