// =====================================================================
// Bloom：拡大パス（3×3テントフィルタ・アップサンプル）
//
//   1段小さいテクスチャを読み、1段大きいテクスチャへ「加算ブレンド」で重ねる。
//   書き込み先にはその段の縮小結果がすでに入っているので、
//   一番小さい段から順に戻っていくと、すべての段のにじみが積み重なる。
//   （小さい段ほど1テクセルが画面上で大きいので、遠くまで届くにじみになる）
// =====================================================================

cbuffer BloomParams : register(b9)
{
    float2 g_SrcTexelSize; // 読み込み元（小さい段）の1テクセルのUV幅
    float g_Threshold; // 縮小パス用（ここでは未使用）
    float g_Knee; // 縮小パス用（ここでは未使用）
    float g_IsFirstPass; // 縮小パス用（ここでは未使用）
    float g_FilterRadius; // テントの広がり（テクセル単位の倍率）
    float2 g_Padding;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = float4(input.Position, 1.0);
    output.TexCoord = input.TexCoord;
    return output;
}

Texture2D srcTexture : register(t0);
SamplerState linearClamp : register(s0);

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float2 uv = input.TexCoord;
    float2 r = g_SrcTexelSize * g_FilterRadius;

    // 3×3のテント（重み 1 2 1 / 2 4 2 / 1 2 1、合計16）
    float3 sum = 0;
    sum += srcTexture.Sample(linearClamp, uv + float2(-r.x, -r.y)).rgb * 1.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(0.0, -r.y)).rgb * 2.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(r.x, -r.y)).rgb * 1.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(-r.x, 0.0)).rgb * 2.0;
    sum += srcTexture.Sample(linearClamp, uv).rgb * 4.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(r.x, 0.0)).rgb * 2.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(-r.x, r.y)).rgb * 1.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(0.0, r.y)).rgb * 2.0;
    sum += srcTexture.Sample(linearClamp, uv + float2(r.x, r.y)).rgb * 1.0;

    return float4(sum / 16.0, 1.0);
}
