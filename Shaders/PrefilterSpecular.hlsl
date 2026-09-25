// =========================================================================
// PrefilterSpecular.hlsl (VS / PS 一体型)
// スカイボックスのキューブマップを Specular IBL 用に畳み込み、
// roughnessごとに異なるミップレベルへ「ボケ具合の違う環境マップ」として焼き込む。
// IrradianceConvolution.hlsl と同じく、実行はゲーム起動時に一度だけ。
// mip0=roughness0.0（ほぼ鏡面）～ 最終mip=roughness1.0（最もボケた状態）
// =========================================================================

// 面ごとのView*Projection行列（IrradianceConvolutionと同じ流儀）
cbuffer PerFaceBuffer : register(b3)
{
    matrix g_ViewProjection;
};

// 今焼いているミップに対応するroughness（ミップ単位でしか変わらない）
cbuffer PerMipBuffer : register(b4)
{
    float g_Roughness;
    float3 g_Padding;
};

struct VS_INPUT
{
    float3 position : POSITION;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

TextureCube g_EnvironmentMap : register(t0);
SamplerState g_SamplerLinear : register(s0);

static const float PI = 3.14159265f;
static const uint SAMPLE_COUNT = 64u; // 起動時1回の焼き込みなので多少重くても許容

// =========================================================================
// 頂点シェーダー（IrradianceConvolutionと全く同じ）
// =========================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;
    output.position = mul(float4(input.position, 1.0f), g_ViewProjection);
    output.texCoord = input.position;
    return output;
}

// ---------------------------------------------------------
// 低差異列（Hammersley）：GGXの重点的サンプリングに使う2次元の均等な乱数列
// ---------------------------------------------------------
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// ---------------------------------------------------------
// GGX分布に沿ってハーフベクトルHをサンプリングする
// roughnessが大きいほど、Nから大きく外れた方向も出やすくなる（＝広いボケ方に対応）
// ---------------------------------------------------------
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // 球面座標→tangent空間
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // tangent空間からワールド空間へ（IrradianceConvolutionと同じ基底の作り方）
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 sampleVec = right * H.x + up * H.y + N * H.z;
    return normalize(sampleVec);
}

// =========================================================================
// ピクセルシェーダー
// N=V=R（視線方向は考慮せず、法線方向そのものを反射方向とみなす簡易版）という
// 前提のもと、GGXの重点的サンプリングで環境マップを畳み込む
// =========================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float3 N = normalize(input.texCoord);
    float3 V = N;
    float roughness = g_Roughness;

    float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    for (uint i = 0u; i < SAMPLE_COUNT; i++)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f)
        {
            // NdotLで重み付けして合算（寄与の大きい方向を優先）
            prefilteredColor += g_EnvironmentMap.SampleLevel(g_SamplerLinear, L, 0.0f).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = (totalWeight > 0.0f) ? (prefilteredColor / totalWeight) : prefilteredColor;

    return float4(prefilteredColor, 1.0f);
}
