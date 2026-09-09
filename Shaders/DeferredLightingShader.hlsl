// =========================================================
// ディファードライティング + SSAO合成ピクセルシェーダー
// =========================================================

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
Texture2D gBufferAlbedo : register(t8); // G-Buffer 0: 色(rgb) + metallic(a)
Texture2D gBufferNormal : register(t9); // G-Buffer 1: 法線(rgb) + roughness(a)
Texture2D gBufferPosition : register(t10); // G-Buffer 2: ワールド座標
Texture2D gBufferDepth : register(t11); // G-Bufferのジオメトリパスで書かれた本物の深度
Texture2D txSSAOBlur : register(t12); // ブラー済みの完成SSAOマップ

SamplerState samLinear : register(s0);
SamplerState samPoint : register(s3);

// シャドウマップ関連（既存の仕組みを維持）
Texture2D shadowMap : register(t3);
TextureCube shadowCubeMap : register(t4);
SamplerComparisonState shadowSampler : register(s1);
SamplerState shadowCubeSampler : register(s2);

static const float PI = 3.14159265f;

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
// Cook-Torrance BRDF の3要素
// ---------------------------------------------------------

// D項：法線分布関数（GGX / Trowbridge-Reitz）
// ハーフベクトルHが法線Nにどれだけ集中しているか＝ハイライトの「形」を決める
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001f);
}

// G項：幾何減衰（Schlick近似によるGGX版）
// 微小凹凸同士が互いを遮蔽/影にする効果。1灯・1方向あたりの遮蔽率
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f; // 直接光用のkの近似（IBLの場合は式が変わる）

    float denom = NdotV * (1.0f - k) + k;
    return NdotV / max(denom, 0.0001f);
}

// 視線方向・光源方向それぞれの遮蔽を掛け合わせて最終的な幾何減衰にする
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// F項：フレネル反射（Schlick近似）
// 見る角度が浅くなるほど反射率が上がる現象。F0は「真正面から見たときの反射率」
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
PS_OUTPUT PS(PS_INPUT input)
{
    // --- 1. G-Buffer から現在のピクセルの情報をサンプリング ---
    float4 albedoData = gBufferAlbedo.Sample(samLinear, input.Tex);
    float4 normalData = gBufferNormal.Sample(samLinear, input.Tex);
    float4 positionData = gBufferPosition.Sample(samLinear, input.Tex);

    // 何も描かれていないピクセルはライト計算をスキップ
    if (positionData.w == 0.0f)
    {
        discard;
    }

    float3 worldPos = positionData.xyz;
    float3 albedo = albedoData.rgb;
    float metallic = albedoData.a; //  G-Bufferから復元
    float roughness = normalData.a; //  G-Bufferから復元
    roughness = max(roughness, 0.045f); // 0だとD項が発散するので下限を設ける

    float3 N = normalize(normalData.xyz * 2.0f - 1.0f);
    float3 V = normalize(vEyePos.xyz - worldPos);
    float NdotV = max(dot(N, V), 0.0f);

    // SSAOによる遮蔽率を取得
    float ssao = txSSAOBlur.Sample(samPoint, input.Tex).r;

    // 非金属は反射率0.04（一般的な誘電体の目安）、金属はアルベド自体を反射色として使う
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 Lo = float3(0.0f, 0.0f, 0.0f); // 直接光の合計（Outgoing radiance）

    for (int i = 0; i < lightCount; i++)
    {
        float3 L;
        if (lights[i].type == 0) // Directional
        {
            L = normalize(-lights[i].direction.xyz);
        }
        else // Point
        {
            L = normalize(lights[i].position.xyz - worldPos);
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
        else
        {
            shadow = ShadowCalculation_Point(worldPos, lights[i].position.xyz, lights[i].farPlane);
        }

        float3 radiance = lights[i].color.xyz * lights[i].intensity * attenuation * shadow;

        float3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0f);

        // --- Cook-Torrance の3要素を計算 ---
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

        // 鏡面反射項：D * G * F / (4 * NdotV * NdotL)
        float3 numerator = D * G * F;
        float denominator = 4.0f * NdotV * NdotL + 0.0001f;
        float3 specular = numerator / denominator;

        // エネルギー保存：反射に回った分(F)は拡散に使えない。
        // さらに金属は拡散反射そのものを持たないので (1-metallic) を掛ける
        float3 kD = (float3(1.0f, 1.0f, 1.0f) - F) * (1.0f - metallic);
        float3 diffuse = kD * albedo / PI;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // 環境光（IBL未実装のため、簡易な定数アンビエントで代用。SSAOで隙間を暗くする）
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * ssao;

    float3 finalColor = ambient + Lo;

    // --- ブルーム用のマルチレンダーターゲット出力 ---
    PS_OUTPUT output;
    output.Color = float4(finalColor, 1.0f);

    float brightness = dot(finalColor, float3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0f)
    {
        output.Bright = float4(finalColor, 1.0f);
    }
    else
    {
        output.Bright = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // --- 深度の転写（値の変更なし） ---
    output.Depth = gBufferDepth.Sample(samPoint, input.Tex).r;

    return output;
}
