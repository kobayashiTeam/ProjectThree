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

// LightDataの1ライト分
struct LightData
{
    float4 position;
    float4 direction;
    float4 color;
    matrix lightSpaceMatrix;
    int type;
    float intensity;
    float2 padding;
};

// LightBufferCB全体
cbuffer LightBuffer : register(b3)
{
    LightData lights[4]; // MAX_LIGHTSの数値を直接書く
    int lightCount;
    float3 padding;
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
    float3 WorldPos : TEXCOORD2; // ワールド空間でのピクセルの位置 POSITIOn
    float4 LightSpacePos : TEXCOORD1; // ← 追加
};

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// 追加
Texture2D shadowMap : register(t3);
SamplerComparisonState shadowSampler : register(s1);

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
    
    // VSでLightSpacePosを計算して渡す
    output.LightSpacePos = mul(worldPos, lights[0].lightSpaceMatrix);
    
    return output;
}


// ShadowCalculation関数
float ShadowCalculation(float4 lightSpacePos)
{
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5f + 0.5f;
    shadowUV.y = -projCoords.y * 0.5f + 0.5f; // D3D11はY反転
    float currentDepth = projCoords.z;
    return shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, currentDepth - 0.005f);//0.005
   
}
// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color * vMaterialColor;

    // ループの外で合計を初期化
    float3 totalLight = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < lightCount; i++)
    {
        float3 lightVec = lights[i].position.xyz - input.WorldPos;
        float distance = length(lightVec);
        float3 lightDir = normalize(lightVec);

        float attenuation = 1.0f / (vAttenuation.x +
                                    vAttenuation.y * distance +
                                    vAttenuation.z * (distance * distance));
        
        // PSのループ内で影を反映
        float shadow = ShadowCalculation(input.LightSpacePos);

        // Ambient
        float3 ambient = 0.2f * lights[i].color.xyz;

        // Diffuse
        float3 normal = normalize(input.Normal);
        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = diff * lights[i].color.xyz;

        // Specular
        float3 viewDir = normalize(vEyePos.xyz - input.WorldPos);
        float3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
        float3 specular = 0.5f * spec * lights[i].color.xyz;

        ambient *= attenuation;
        diffuse *= attenuation*shadow;
        specular *= attenuation*shadow;

        // ループのたびに加算
        totalLight += (ambient + diffuse) * objectColor.xyz + specular;
    }
    
    return float4(totalLight, objectColor.a);
}


