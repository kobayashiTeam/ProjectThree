// =========================================================
// GPUパーティクルの描画
//
// 頂点バッファも InputLayout も使わない。C++ は Draw(粒の数) と言うだけで、
//   VS ：SV_VertexID（0, 1, 2, ...）を番号として、粒のバッファ（StructuredBuffer）から読む
//   GS ：点1つ → カメラを向いた四角形1枚（ビュー空間で4隅に広げる）
//   PS ：丸くぼかして、加算で重ねる
// 1つのファイルに VS / GSmain / PS の3つの入口がある
// （VS/PS は ShaderManager::GetOrCreate、GS は loadGeometryShader が同じファイルから読む）
// =========================================================

// ParticleSystem::Particle と同じ並び（ParticleUpdateCS.hlsl にも同じものがある）
struct Particle
{
    float3 position;
    float age; // 負の値＝まだ生まれていない（待ち時間）
    float3 velocity;
    float lifetime;
};

// RW なし ＝ 読むだけ。C++ 側では、同じバッファの SRV を VS の t0 につなぐ
StructuredBuffer<Particle> g_particles : register(t0);

cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

// ParticleSystem::DrawCB と同じ並び（GS と PS が読む）
cbuffer ParticleDrawCB : register(b10)
{
    float g_size; // 板の一辺の半分（ワールドの長さ）
    float g_intensity; // 明るさの倍率
    float2 g_pad;
};

struct VSOut
{
    float3 WorldPos : TEXCOORD0;
    float LifeRatio : TEXCOORD1; // age / lifetime（0：生まれたて → 1：寿命の終わり）。負なら未誕生
};

struct GSOut
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

// ----------------------------------------------------------
// Vertex Shader：番号 → 粒を読むだけ
// ----------------------------------------------------------
VSOut VS(uint id : SV_VertexID)
{
    Particle p = g_particles[id];
    VSOut o;
    o.WorldPos = p.position;
    o.LifeRatio = (p.age < 0.0f) ? -1.0f : saturate(p.age / p.lifetime);
    return o;
}

// 寿命の割合から色を決める（明るい黄白 → 橙 → 暗い赤、最後は消える）
// HDR の画面に描くので、1 を超える明るさにしておくと Bloom でにじむ
float3 LifeColor(float t)
{
    float3 young = float3(4.0f, 3.0f, 1.2f);
    float3 mid = float3(2.0f, 0.6f, 0.1f);
    float3 old = float3(0.6f, 0.05f, 0.02f);
    float3 c = (t < 0.4f) ? lerp(young, mid, t / 0.4f) : lerp(mid, old, (t - 0.4f) / 0.6f);
    float fade = 1.0f - t;
    return c * fade * fade;
}

// ----------------------------------------------------------
// Geometry Shader：点1つ → カメラ向きの四角形1枚
// PointSpriteGS.hlsl はワールドのXY平面に固定して広げていたが、
// こちらはビュー空間（カメラが原点で +Z を向いている空間）で広げるので、常にカメラの正面を向く
// ----------------------------------------------------------
[maxvertexcount(4)]
void GSmain(point VSOut input[1], inout TriangleStream<GSOut> stream)
{
    // まだ生まれていない粒は、何も出さない（＝描かれない）
    if (input[0].LifeRatio < 0.0f)
        return;

    float4 viewCenter = mul(float4(input[0].WorldPos, 1.0f), mView);
    float h = g_size;
    float4 color = float4(LifeColor(input[0].LifeRatio) * g_intensity, 1.0f);

    // TriangleStrip なので Z字の順：左上 → 右上 → 左下 → 右下
    const float2 offsets[4] = { float2(-h, h), float2(h, h), float2(-h, -h), float2(h, -h) };
    const float2 uvs[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        GSOut v;
        v.Pos = mul(viewCenter + float4(offsets[i], 0.0f, 0.0f), mProjection);
        v.UV = uvs[i];
        v.Color = color;
        stream.Append(v);
    }
}

// ----------------------------------------------------------
// Pixel Shader：中心が明るく、縁で 0 になる丸
// 加算合成なので、0 のところは何も足さない（discard しなくても見た目は同じ）
// ----------------------------------------------------------
float4 PS(GSOut input) : SV_Target
{
    float r = length(input.UV * 2.0f - 1.0f);
    float falloff = saturate(1.0f - r);
    falloff *= falloff;
    if (falloff <= 0.0f)
    {
        discard;
    }
        return float4(input.Color.rgb * falloff, 1.0f);
    }
