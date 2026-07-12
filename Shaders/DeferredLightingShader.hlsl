// =========================================================
// ディファードライティング ＋ SSAO合成ピクセルシェーダー
// =========================================================

// ---------------------------------------------------------
// 定数バッファ（既存の構造をそのまま維持）
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
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
    float4 Bright : SV_Target1;
    float Depth : SV_Depth;
};

// ---------------------------------------------------------
// テクスチャ・サンプラースロット設定
// ---------------------------------------------------------
Texture2D gBufferAlbedo : register(t8); // G-Buffer 0: 色
Texture2D gBufferNormal : register(t9); // G-Buffer 1: 法線
Texture2D gBufferPosition : register(t10); // G-Buffer 2: ワールド座標
Texture2D gBufferDepth : register(t11); // G-Bufferのジオメトリパスで書かれた本物の深度
Texture2D txSSAOBlur : register(t12); // ★追加: ブラー済みの完成版SSAOマップ

SamplerState samLinear : register(s0);
SamplerState samPoint : register(s3); // 深度およびSSAOサンプリング用（値を歪ませないため）

// シャドウマップ関連（既存の指定スロットを維持）
Texture2D shadowMap : register(t3);
TextureCube shadowCubeMap : register(t4);
SamplerComparisonState shadowSampler : register(s1);
SamplerState shadowCubeSampler : register(s2);

// ---------------------------------------------------------
// 頂点シェーダー (VS)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    output.Pos = input.Pos;
    output.Tex = input.Tex;
    return output;
}

// ---------------------------------------------------------
// シャドウ計算（既存のロジックをそのまま維持）
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
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
PS_OUTPUT PS(PS_INPUT input)
{
    // --- 1. G-Buffer から現在のピクセルの「材料」をサンプリング ---
    float4 albedoData = gBufferAlbedo.Sample(samLinear, input.Tex);
    float4 normalData = gBufferNormal.Sample(samLinear, input.Tex);
    float4 positionData = gBufferPosition.Sample(samLinear, input.Tex);

    // 何も描かれていない背景ピクセルはライト計算をスキップ
    if (positionData.w == 0.0f)
    {
        discard;
    }

    float3 worldPos = positionData.xyz;
    float4 objectColor = albedoData;

    float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);
    float3 viewDir = normalize(vEyePos.xyz - worldPos);

    // ★追加：SSAOマップからオクルージョン値（0.0～1.0）をサンプリング
    // 値がブレるのを防ぐため、深度と同じ samPoint サンプラーを使用します
    float ssao = txSSAOBlur.Sample(samPoint, input.Tex).r;

    // --- 2. ライト計算（SSAOを環境光に適用） ---
    // 全体環境光（globalAmbient）に対して SSAO値を掛け算し、角や隙間を暗くします
    float3 globalAmbient = float3(0.1f, 0.1f, 0.1f) * objectColor.xyz * ssao;
    float3 totalDirectLight = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < lightCount; i++)
    {
        float3 lightDir;
        if (lights[i].type == 0)
        {
            lightDir = normalize(-lights[i].direction.xyz);
        }
        else if (lights[i].type == 1)
        {
            lightDir = normalize(lights[i].position.xyz - worldPos);
        }

        float attenuation = 1.0f;
        if (lights[i].type == 1)
        {
            float distance = length(lights[i].position.xyz - worldPos);
            attenuation = 1.0f /
            (vAttenuation.x + vAttenuation.y * distance + vAttenuation.z * distance * distance);
        }

        float shadow = 1.0f;
        if (lights[i].type == 0)
        {
            float4 lightSpacePos = mul(float4(worldPos, 1.0f), lights[i].lightSpaceMatrix);
            shadow = ShadowCalculation_Directional(lightSpacePos);
        }
        else if (lights[i].type == 1)
        {
            shadow = ShadowCalculation_Point(worldPos, lights[i].position.xyz, lights[i].farPlane);
        }

        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = diff * lights[i].color.xyz * lights[i].intensity;

        float3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
        float3 specular = 0.5f * spec * lights[i].color.xyz * lights[i].intensity;

        diffuse *= attenuation * shadow;
        specular *= attenuation * shadow;

        totalDirectLight += diffuse * objectColor.xyz + specular;
    }

    // 環境光（SSAO適用済）と直接光を合計
    float3 finalColor = globalAmbient + totalDirectLight;

    // --- 3. ブルーム用マルチレンダーターゲット出力 ---
    // (※ブルームの光漏れ判定[Bright]にも、SSAOが反映された最終カラーを用います)
    PS_OUTPUT output;
    output.Color = float4(finalColor, objectColor.a);
    // デバッグ: SSAOの値だけを出力してみる
    //output.Color = float4(ssao, ssao, ssao, 1.0f);

    float brightness = dot(finalColor, float3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0f)
    {
        output.Bright = float4(finalColor, 1.0f);
    }
    else
    {
        output.Bright = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // --- 4. 深度の転写（変更なし） ---
    output.Depth = gBufferDepth.Sample(samPoint, input.Tex).r;

    return output;
}