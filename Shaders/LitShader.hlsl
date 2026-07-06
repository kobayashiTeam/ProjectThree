// ---------------------------------------------------------
// 定数バッファの分割
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

struct LightData
{
    float4 position;
    float4 direction;
    float4 color;
    matrix lightSpaceMatrix;
    int type;
    float intensity;
    float farPlane;
    float padding;
};

cbuffer LightBuffer : register(b3)
{
    LightData lights[4];
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
    float3 WorldPos : TEXCOORD2;
    float4 LightSpacePos : TEXCOORD1;
};

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

Texture2D shadowMap : register(t3);
TextureCube shadowCubeMap : register(t4);

SamplerComparisonState shadowSampler : register(s1);
SamplerState shadowCubeSampler : register(s2);

// ---------------------------------------------------------
// 頂点シェーダー
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;

    float4 worldPos = mul(input.Pos, mModel);
    output.WorldPos = worldPos.xyz;

    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);

    output.Normal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);

    output.Color = input.Color;
    output.Tex = input.Tex;

    // DirectionalライトのlightSpacePos（lights[0]固定）
    output.LightSpacePos = float4(0, 0, 0, 0);
    for (int i = 0; i < lightCount; i++)
    {
        if (lights[i].type == 0)
        {
            output.LightSpacePos = mul(worldPos, lights[i].lightSpaceMatrix);
            break;
        }
    }

    return output;
}

// ---------------------------------------------------------
// シャドウ計算
// ---------------------------------------------------------
float ShadowCalculation_Directional(float4 lightSpacePos)
{
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5f + 0.5f;
    shadowUV.y = -projCoords.y * 0.5f + 0.5f;
    float currentDepth = projCoords.z;
    return shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV, currentDepth - 0.005f);
}

float ShadowCalculation_Point(float3 worldPos, float3 lightPos, float farPlane)
{
    float3 lightToFrag = worldPos - lightPos;
    float currentDepth = length(lightToFrag);
    float closestDepth = shadowCubeMap.Sample(shadowCubeSampler, lightToFrag).r * farPlane;
    
    return (currentDepth - 0.05f > closestDepth) ? 0.0f : 1.0f;
}

// ---------------------------------------------------------
// ピクセルシェーダー
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color * vMaterialColor;

    float3 normal = normalize(input.Normal);
    float3 viewDir = normalize(vEyePos.xyz - input.WorldPos);

    // 【修正】アンビエントはループの外で1回だけ（ベースの暗さを決める）
    // シーン全体の環境光として、例えば 0.1 程度の強さにする
    float3 globalAmbient = float3(0.1f, 0.1f, 0.1f) * objectColor.xyz;
    
    float3 totalDirectLight = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < lightCount; i++)
    {
        // --- (ライト方向と減衰の計算はそのまま) ---
        float3 lightDir;
        if (lights[i].type == 0)
        {
            lightDir = normalize(-lights[i].direction.xyz);
        }
        else if (lights[i].type==1)
        {
            lightDir = normalize(lights[i].position.xyz - input.WorldPos);
        }

        float attenuation = 1.0f;
        if (lights[i].type == 1)
        {
            float distance = length(lights[i].position.xyz - input.WorldPos);
            attenuation = 1.0f / 
            (vAttenuation.x + vAttenuation.y * distance + vAttenuation.z * distance * distance);
        }

        // --- (シャドウ計算はそのまま) ---
        float shadow = 1.0f; // デフォルトは影なし(1.0)
        if (lights[i].type == 0)
        {
            shadow = ShadowCalculation_Directional(input.LightSpacePos);
        }
        else if (lights[i].type == 1)
        {
            shadow = ShadowCalculation_Point(
            input.WorldPos, lights[i].position.xyz, lights[i].farPlane);
        }

        // --- ライティング計算（アンビエントを排除） ---
        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = diff * lights[i].color.xyz * lights[i].intensity; // intensityも考慮

        // Specular
        float3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
        float3 specular = 0.5f * spec * lights[i].color.xyz * lights[i].intensity;

        // 減衰とシャドウを適用
        diffuse *= attenuation * shadow;
        specular *= attenuation * shadow;

        // 直射光のみを蓄積
        totalDirectLight += diffuse * objectColor.xyz + specular;
    }

    // 最終カラー ＝ 全体の環境光 ＋ 蓄積された直射光
    float3 finalColor = globalAmbient + totalDirectLight;
    //test
    //finalColor *= 15.0f;
    return float4(finalColor, objectColor.a);
}