// ---------------------------------------------------------
// 定数バッファ
// ---------------------------------------------------------
// スロット0：フレーム単位で共通（カメラやライトの情報）- 変更なし！
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

// ★スロット1（PerObjectBuffer）は使用しないため削除

// スロット2：マテリアル単位で共通 - 変更なし！
cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor;
};

// ---------------------------------------------------------
// 入出力構造体
// ---------------------------------------------------------
struct VS_INPUT
{
    // スロット0 ~ 3：頂点属性データ（通常描画と同じ）
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;

    // ★スロット4：インスタンスデータ（C++側の入力レイアウトでセマンティクスインデックスを0~3に分けたもの）
    float4 InstMatrixRow0 : INSTANCE_WORLD0;
    float4 InstMatrixRow1 : INSTANCE_WORLD1;
    float4 InstMatrixRow2 : INSTANCE_WORLD2;
    float4 InstMatrixRow3 : INSTANCE_WORLD3;
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

//// ---------------------------------------------------------
//// 頂点シェーダー (VS)
//// ---------------------------------------------------------
//PS_INPUT VS(VS_INPUT input)
//{
//    PS_INPUT output = (PS_INPUT) 0;
    
//    // ★4本のfloat4から、このインスタンス固有の4x4ワールド行列を再構築
//    float4x4 instanceModelMatrix = float4x4(
//        input.InstMatrixRow0,
//        input.InstMatrixRow1,
//        input.InstMatrixRow2,
//        input.InstMatrixRow3
//    );
    
//    // ★mModelの代わりに、再構築したinstanceModelMatrixを使ってワールド座標を計算
//    float4 worldPos = mul(input.Pos, instanceModelMatrix);//input.Pos, instanceModelMatrix
//    output.WorldPos = worldPos.xyz;
    
//    output.Pos = mul(worldPos, mView);
//    output.Pos = mul(output.Pos, mProjection);
    
//    // ★法線ベクトルもインスタンス行列でワールド変換
//    output.Normal = mul(float4(input.Normal, 0.0f), instanceModelMatrix).xyz;
//    output.Normal = normalize(output.Normal);
    
//    output.Color = input.Color;
//    output.Tex = input.Tex;
    
//    return output;
//}

//// ---------------------------------------------------------
//// ピクセルシェーダー (PS) - 元のコードから変更なし！
//// ---------------------------------------------------------
//float4 PS(PS_INPUT input) : SV_Target
//{
//    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    
//    float4 objectColor = texColor * input.Color * vMaterialColor;
    
//    float3 lightVec = vLightPos.xyz - input.WorldPos;
//    float distance = length(lightVec);
//    float3 lightDir = normalize(lightVec);
    
//    float attenuation = 1.0f / (vAttenuation.x +
//                                 vAttenuation.y * distance +
//                                 vAttenuation.z * (distance * distance));
    
//    // 1. Ambient (環境光)
//    float ambientStrength = 0.2f;
//    float3 ambient = ambientStrength * vLightColor.xyz;
    
//    // 2. Diffuse (拡散反射光)
//    float3 normal = normalize(input.Normal);
//    float diff = max(dot(normal, lightDir), 0.0f);
//    float3 diffuse = diff * vLightColor.xyz;
    
//    // 3. Specular (鏡面反射光)
//    float specularStrength = 0.5f;
//    float3 viewDir = normalize(vEyePos.xyz - input.WorldPos);
//    float3 halfwayDir = normalize(lightDir + viewDir);
    
//    float spec = pow(max(dot(normal, halfwayDir), 0.0f), 32.0f);
//    float3 specular = specularStrength * spec * vLightColor.xyz;
    
//    ambient *= attenuation;
//    diffuse *= attenuation;
//    specular *= attenuation;
    
//    float3 finalColor = (ambient + diffuse) * objectColor.xyz + specular;
    
//    return float4(finalColor, objectColor.a);
//}


// ---------------------------------------------------------
// 頂点シェーダー (VS)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    float4x4 instanceModelMatrix = float4x4(
        input.InstMatrixRow0,
        input.InstMatrixRow1,
        input.InstMatrixRow2,
        input.InstMatrixRow3
    );
    
    // ワールド→ビュー→プロジェクション変換のみ
    float4 worldPos = mul(input.Pos, instanceModelMatrix);
    output.Pos = mul(worldPos, mView);
    output.Pos = mul(output.Pos, mProjection);
    
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
    
    // ライティング計算なし。テクスチャ・頂点カラー・マテリアルカラーの積をそのまま出力
    return texColor * input.Color * vMaterialColor;
}