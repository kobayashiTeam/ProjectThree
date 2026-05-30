// ---------------------------------------------------------
// 定数バッファ（C++側と完全一致させる）
// ---------------------------------------------------------
cbuffer ConstantBuffer : register(b0)
{
    matrix mModel;
    matrix mView;
    matrix mProjection;
    float4 vLightDir;
    float4 vLightColor;
    float4 vEyePos; // ★追加：カメラの位置
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
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color;
    
    // 1. Ambient (環境光)
    float ambientStrength = 0.2f;
    float3 ambient = ambientStrength * vLightColor.xyz;
    
    // 2. Diffuse (拡散反射光)
    float3 normal = normalize(input.Normal);
    float3 lightDir = -vLightDir.xyz;
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = diff * vLightColor.xyz;
    
    // 3. Specular (鏡面反射光：Blinn-Phongモデル) ★新規追加
    float specularStrength = 0.5f; // ハイライトの強さ
    float3 viewDir = normalize(vEyePos.xyz - input.WorldPos); // ピクセルからカメラへの方向
    float3 halfwayDir = normalize(lightDir + viewDir); // ライト方向と視点方向のハーフベクトル
    
    // 法線とハーフベクトルの内積をとり、32乗してハイライトを鋭くする（この数値が大きいほどツルツルになる）
    float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
    float3 specular = specularStrength * spec * vLightColor.xyz;
    
    // 4. 最終的な色の結合（テクスチャ・頂点色には specular は乗算せず、最後に足す）
    float3 finalColor = (ambient + diffuse) * objectColor.xyz + specular;
    
    return float4(finalColor, objectColor.a);
}