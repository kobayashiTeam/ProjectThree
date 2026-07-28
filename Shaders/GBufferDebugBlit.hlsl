// GBufferDebugBlit.hlsl
// Scene3（遅延レンダリング）用：Gバッファ(Albedo/Normal/Depth)の中身を、
// トーンマッピングやガンマ補正を一切かけずにそのままバックバッファへ映すだけの単純なシェーダー。
// ScreenBlit.hlslと違い「見た目を整える」ことが目的ではなく「生データの検証」が目的。

struct VS_INPUT
{
    float3 Position : POSITION; // NDC座標 (-1.0 ~ 1.0)
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

Texture2D sourceTexture : register(t0);
SamplerState linearSampler : register(s0);

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    // 補正なしでそのまま出力。
    // Albedo/Normalは4チャンネルの色としてそのまま意味のある絵になる。
    // Depth（R32_FLOATの1チャンネル）は緑・青成分が0埋めされるため赤系の濃淡として見える。
    float4 raw = sourceTexture.Sample(linearSampler, input.TexCoord);
    return float4(raw.rgb, 1.0);
}
