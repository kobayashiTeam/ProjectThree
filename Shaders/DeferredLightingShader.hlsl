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

// ※第2パスでは個別メッシュをレンダリングしないため、
//  PerObjectBuffer (b1) や PerMaterialBuffer (b2) は使用しません。

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
// C++側からは画面全体の四角形（2ポリゴン）の頂点データを流し込みます
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0; // スクリーン全体を覆うUV座標
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

// 出力（元のシェーダー同様、Bloom用の高輝度抽出付きマルチターゲット）
struct PS_OUTPUT
{
    float4 Color : SV_Target0; // バックバッファへの最終カラー
    float4 Bright : SV_Target1; // Bloom高輝度抽出用
    float Depth : SV_Depth; // ← 追加
};

// ---------------------------------------------------------
// テクスチャ・サンプラースロット設定
// ---------------------------------------------------------
// 既存の txDiffuse(t0), shadowMap(t3), shadowCubeMap(t4) と被らないよう配置
Texture2D gBufferAlbedo : register(t8); // G-Buffer 0: 色
Texture2D gBufferNormal : register(t9); // G-Buffer 1: 法線
Texture2D gBufferPosition : register(t10); // G-Buffer 2: ワールド座標

SamplerState samLinear : register(s0);

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
    
    // 入力頂点はC++側で射影空間（X: -1~1, Y: -1~1）の四角形を想定
    output.Pos = input.Pos;
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// シャドウ計算（既存のロジックをそのまま移植）
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

    // 深度バッファをすり抜けた背景（Skybox等を描かない場合）はライト計算をスキップ
    if (positionData.w == 0.0f)
    {
        discard;
    }

    float3 worldPos = positionData.xyz;
    float4 objectColor = albedoData; // 元の物体の色（テクスチャ×マテリアルカラー反映済み）

    // [0, 1]にパッキングされていた法線ベクトルを [-1, 1] の空間に復元
    float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);
    float3 viewDir = normalize(vEyePos.xyz - worldPos);

    // --- 2. ライト計算（元のループ処理をそっくりそのまま実行） ---
    float3 globalAmbient = float3(0.1f, 0.1f, 0.1f) * objectColor.xyz;
    float3 totalDirectLight = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < lightCount; i++)
    {
        float3 lightDir;
        if (lights[i].type == 0) // Directional
        {
            lightDir = normalize(-lights[i].direction.xyz);
        }
        else if (lights[i].type == 1) // Point
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

        // --- シャドウ計算（必要な座標系はここでその都度生成） ---
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

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = diff * lights[i].color.xyz * lights[i].intensity;

        // Specular
        float3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
        float3 specular = 0.5f * spec * lights[i].color.xyz * lights[i].intensity;

        diffuse *= attenuation * shadow;
        specular *= attenuation * shadow;

        totalDirectLight += diffuse * objectColor.xyz + specular;
    }

    // 最終カラー合成
    float3 finalColor = globalAmbient + totalDirectLight;

    // --- 3. ブルーム用マルチレンダーターゲット出力 ---
    PS_OUTPUT output;
    output.Color = float4(finalColor, objectColor.a);

    // 輝度（明るさ）を計算してBloom抽出
    float brightness = dot(finalColor, float3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0f)
    {
        output.Bright = float4(finalColor, 1.0f);
    }
    else
    {
        output.Bright = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // --- 4. 深度の再構成（Depth Resolve） ---
    // ワールド座標を View→Projection で再度クリップ空間に変換し、
    // 「本来このオブジェクトが持っていたはずの深度」をSV_Depthとして書き戻す
    float4 clipPos = mul(float4(worldPos, 1.0f), mView);
    clipPos = mul(clipPos, mProjection);
    output.Depth = clipPos.z / clipPos.w; // NDC深度 (0.0〜1.0)

    return output;
}