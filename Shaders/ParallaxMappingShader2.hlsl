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

// パララックス用の係数を追加できるように PerMaterialBuffer を拡張
cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor;
    float fHeightScale; // ★パララックスの深さの係数 (C++側から0.02〜0.05程度を渡す)
    float3 vMaterialPadding;
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
    float3 Tangent : TANGENT;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : TEXCOORD2;
    float4 LightSpacePos : TEXCOORD1;
    float3 WorldNormal : NORMAL;
    float3 WorldTangent : TANGENT;
};

// ---------------------------------------------------------
// テクスチャ・サンプラー
// ---------------------------------------------------------
Texture2D txDiffuse : register(t0);
Texture2D txNormalHeightMap : register(t1); // ★RGB: 法線, A: ハイトマップ が入った統合テクスチャ
SamplerState samLinear : register(s0);

Texture2D shadowMap : register(t3);
TextureCube shadowCubeMap : register(t4);

SamplerComparisonState shadowSampler : register(s1);
SamplerState shadowCubeSampler : register(s2);

// ---------------------------------------------------------
// 頂点シェーダー (VS) - 法線マップの時と共通でOK
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;

    float4 worldPos = mul(input.Pos, mModel);
    output.WorldPos = worldPos.xyz;

    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);

    output.WorldNormal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);
    output.WorldTangent = normalize(mul(float4(input.Tangent, 0.0f), mModel).xyz);

    output.Color = input.Color;
    output.Tex = input.Tex;

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
// ★ パララックスマッピング関数（UVをズラすコアロジック）
// ---------------------------------------------------------
float2 ParallaxMapping(float2 texCoords, float3 viewDirTangent)
{
    // 現在のUV位置のハイトマップ値をサンプリング (Aチャンネルから取得)
    float height = txNormalHeightMap.Sample(samLinear, texCoords).a;
    
    // 視線の傾きに合わせてUVのズレを計算
    // LearnOpenGLと同じく「1.0 - height」にするか「height」にするかは
    // 白黒画像の「どっちが凹か」によって反転させてください。
    // ここでは 白(1.0)が凸、黒(0.0)が凹とし、下に沈み込ませる計算にしています。
    float2 p = viewDirTangent.xy / viewDirTangent.z * (height * fHeightScale);
    
    // 元のUV座標から引いて補正後のUVを返す
    return texCoords - p;
}

// ---------------------------------------------------------
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // 1. TBN空間（接空間）の基底ベクトルを計算
    float3 N = normalize(input.WorldNormal);
    float3 T = normalize(input.WorldTangent);
    T = normalize(T - dot(T, N) * N); // グラム・シュミット直交化
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N); // 接空間 -> ワールド空間

    // 2. ★ワールド空間の視線ベクトルを、接空間（TBN空間）へ逆変換する
    float3 viewDirWorld = normalize(vEyePos.xyz - input.WorldPos);
    // TBN行列は直交行列なので、逆行列は転置（transpose）で求められます
    float3x3 worldToTangent = transpose(TBN);
    float3 viewDirTangent = normalize(mul(viewDirWorld, worldToTangent));

    // 3. ★パララックスマッピングを実行し、視差を考慮した新しいUV座標を取得
    float2 offsetTexCoords = ParallaxMapping(input.Tex, viewDirTangent);

    // 4. ★描画がポリゴンのUV境界（0.0〜1.0）を超えた場合、ピクセルを捨てる（お好みで）
    // 壁の端の表現などで不自然に引き延ばされるのを防ぎます
    if (offsetTexCoords.x < 0.0f || offsetTexCoords.x > 1.0f || offsetTexCoords.y < 0.0f || offsetTexCoords.y > 1.0f)
    {
        discard;
    }

    // 5. ★【重要】これ以降はすべて「offsetTexCoords」を使ってサンプリングする！
    float4 texColor = txDiffuse.Sample(samLinear, offsetTexCoords);
    float4 objectColor = texColor * input.Color * vMaterialColor;

    // 統合テクスチャから新しいUVで法線マップの色をサンプリング（XYZ成分）
    float3 normalMapColor = txNormalHeightMap.Sample(samLinear, offsetTexCoords).xyz;
    
    // デコード [-1.0 〜 1.0]
    float3 localNormal = normalMapColor * 2.0f - 1.0f;
    
    // OpenGL形式のノーマルマップ対策（上下が逆なら反転）
    // localNormal.y = -localNormal.y; 

    // 新しいUVから作った法線をワールド空間へ変換
    float3 normal = normalize(mul(localNormal, TBN));

    // ---------------------------------------------------------
    // ライティング計算（ここからは元コードのまま、normalと新しいobjectColorを使用）
    // ---------------------------------------------------------
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
            lightDir = normalize(lights[i].position.xyz - input.WorldPos);
        }

        float attenuation = 1.0f;
        if (lights[i].type == 1)
        {
            float distance = length(lights[i].position.xyz - input.WorldPos);
            attenuation = 1.0f /
            (vAttenuation.x + vAttenuation.y * distance + vAttenuation.z * distance * distance);
        }

        float shadow = 1.0f;
        if (lights[i].type == 0)
        {
            shadow = ShadowCalculation_Directional(input.LightSpacePos);
        }
        else if (lights[i].type == 1)
        {
            shadow = ShadowCalculation_Point(
                input.WorldPos, lights[i].position.xyz, lights[i].farPlane);
        }

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = diff * lights[i].color.xyz * lights[i].intensity;

        // Specular (Blinn-Phong)
        float3 halfwayDir = normalize(lightDir + viewDirWorld);
        float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
        float3 specular = 0.5f * spec * lights[i].color.xyz * lights[i].intensity;

        diffuse *= attenuation * shadow;
        specular *= attenuation * shadow;

        totalDirectLight += diffuse * objectColor.xyz + specular;
    }

    float3 finalColor = globalAmbient + totalDirectLight;
    return float4(finalColor, objectColor.a);
}