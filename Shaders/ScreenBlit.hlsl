// ScreenBlit.hlsl
// オフスクリーンTextureをバックバッファにそのまま描き写す（初学者向け単純コピー）
//inputLauoutは本来最低限のものでいいが、shader側の実装の都合で全シェーダ共通の
//layoutをつかわなければいけない。ここは仕方なく埋める

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

Texture2D sceneTexture : register(t0); // オフスクリーンから来たTexture
SamplerState linearSampler : register(s0);

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = sceneTexture.Sample(linearSampler, input.TexCoord);
    
    // 将来ここに軽い調整を入れる余地を残す
     color.rgb = pow(color.rgb, 1.0 / 2.2); // Gamma修正など
    
    return color;
}