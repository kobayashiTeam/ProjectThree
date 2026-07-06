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
// 入出力構造体（ノーマルマップ用に拡張）
// ---------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 Tangent : TANGENT; // ★C++側の5番目のストリームから受け取る
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 WorldPos : TEXCOORD2;
    float4 LightSpacePos : TEXCOORD1;
    // ★ピクセルシェーダーにワールド空間の「法線」と「接線」を別々で渡す
    float3 WorldNormal : NORMAL;
    float3 WorldTangent : TANGENT;
};

// テクスチャ・サンプラー（t1に法線マップを追加）
Texture2D txDiffuse : register(t0);
Texture2D txNormalMap : register(t1); // ★法線マップ用スロット
SamplerState samLinear : register(s0);

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

    // ワールド座標の計算
    float4 worldPos = mul(input.Pos, mModel);
    output.WorldPos = worldPos.xyz;

    // スクリーン座標の計算
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);

    // ★ワールド空間における「法線」と「接線」をそれぞれ計算してPSへ送る
    output.WorldNormal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);
    output.WorldTangent = normalize(mul(float4(input.Tangent, 0.0f), mModel).xyz);

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
// ピクセルシェーダー (PS)
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // カラーテクスチャとマテリアル色の合成
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color * vMaterialColor;

    // ---------------------------------------------------------
    // ★ ノーマルマッピングによる法線ベクトルの生成
    // ---------------------------------------------------------
    float3 N = normalize(input.WorldNormal);
    float3 T = normalize(input.WorldTangent);
    
    // グラム・シュミットの直交化（頂点補間による歪みを綺麗に補正する）
    T = normalize(T - dot(T, N) * N);
    
    // 外積から従法線（Binormal/Bitangent）を計算
    float3 B = cross(N, T);
    
    // TBN行列の構築 (接空間からワールド空間への変換行列)
    float3x3 TBN = float3x3(T, B, N);

    // 法線マップからサンプリング
    float3 normalMapColor = txNormalMap.Sample(samLinear, input.Tex).xyz;
    
    // カラー値 [0.0 〜 1.0] を ベクトル成分 [-1.0 〜 1.0] へデコード
    float3 localNormal = normalMapColor * 2.0f - 1.0f;
    
    // 【重要】もしOpenGL形式（LearnOpenGLの素材など）の画像を使って
    // 凹凸の上下が逆に見える場合は、以下の行のコメントアウトを解除してください。
    // localNormal.y = -localNormal.y; 

    // 接空間の法線を、TBN行列を用いてワールド空間へ変換する
    float3 normal = normalize(mul(localNormal, TBN));
    // ---------------------------------------------------------

    // 視線方向の計算
    float3 viewDir = normalize(vEyePos.xyz - input.WorldPos);

    // アンビエント（環境光）の計算
    float3 globalAmbient = float3(0.1f, 0.1f, 0.1f) * objectColor.xyz;
    
    // 直射光の蓄積バッファ
    float3 totalDirectLight = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < lightCount; i++)
    {
        // ライト方向の計算
        float3 lightDir;
        if (lights[i].type == 0) // Directional
        {
            lightDir = normalize(-lights[i].direction.xyz);
        }
        else if (lights[i].type == 1) // Point
        {
            lightDir = normalize(lights[i].position.xyz - input.WorldPos);
        }

        // 光の減衰計算（Pointライトのみ）
        float attenuation = 1.0f;
        if (lights[i].type == 1)
        {
            float distance = length(lights[i].position.xyz - input.WorldPos);
            attenuation = 1.0f /
            (vAttenuation.x + vAttenuation.y * distance + vAttenuation.z * distance * distance);
        }

        // シャドウ計算
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

        // --- ライティング計算（差し替えた normal を使用） ---
        // Diffuse (拡散反射)
        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = diff * lights[i].color.xyz * lights[i].intensity;

        // Specular (鏡面反射 - Blinn-Phong)
        float3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
        float3 specular = 0.5f * spec * lights[i].color.xyz * lights[i].intensity;

        // 減衰とシャドウを適用
        diffuse *= attenuation * shadow;
        specular *= attenuation * shadow;

        // 直射光を蓄積
        totalDirectLight += diffuse * objectColor.xyz + specular;
    }

    // 最終的なカラー出力（環境光 ＋ 直射光の総和）
    float3 finalColor = globalAmbient + totalDirectLight;
    //test
    //finalColor *= 8.0f;
    return float4(finalColor, objectColor.a);
}