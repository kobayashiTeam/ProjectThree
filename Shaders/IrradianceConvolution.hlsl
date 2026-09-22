// =========================================================================
// IrradianceConvolution.hlsl (VS / PS 一体型)
// スカイボックスのキューブマップを拡散反射(Diffuse IBL)用に事前積分し、
// 小さな Irradiance キューブマップへ焼き込むためのシェーダー。
// 実行はゲーム起動時に一度だけ（6面をそれぞれレンダーターゲットにして描画）。
// =========================================================================

// 面ごとのView*Projection行列（bakeパス専用、Renderer::Executeとは無関係）
cbuffer PerFaceBuffer : register(b3)
{
    matrix g_ViewProjection;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD0; // ローカル座標＝サンプリング方向としてそのまま使う
};

// 元になる環境キューブマップ（スカイボックス本体）
TextureCube g_EnvironmentMap : register(t0);
SamplerState g_SamplerLinear : register(s0);

static const float PI = 3.14159265f;

// =========================================================================
// 頂点シェーダー
// =========================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;
    output.position = mul(float4(input.position, 1.0f), g_ViewProjection);
    output.texCoord = input.position;
    return output;
}

// =========================================================================
// ピクセルシェーダー
// 半球（法線Nを中心とした上半分）をコサイン重み付きで積算し、
// 「その方向を向いた面に、周囲からどれだけ拡散光が入ってくるか」を求める
// =========================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float3 N = normalize(input.texCoord);

    // Nに直交するtangent基底(right, up)を作る
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    float sampleDelta = 0.05f; // 小さいほど高精度・低速（起動時に一度だけの処理なので許容）
    float nrSamples = 0.0f;

    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
        {
            // 球面座標→tangent空間での方向ベクトル
            float3 tangentSample = float3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));

            // tangent空間からワールド空間（Nを中心とした半球）へ変換
            float3 sampleVec =
                tangentSample.x * right +
                tangentSample.y * up +
                tangentSample.z * N;

            // cos(theta)：ランバートの余弦則（浅い角度ほど寄与が小さい）
            // sin(theta)：球面座標特有の面積補正（極に近いほどサンプルが密集するのを補正）
            irradiance += g_EnvironmentMap.SampleLevel(g_SamplerLinear, sampleVec, 0).rgb
                          * cos(theta) * sin(theta);
            nrSamples += 1.0f;
        }
    }

    irradiance = PI * irradiance / nrSamples;

    return float4(irradiance, 1.0f);
}
