cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

// シャドウマップ1枚ぶんのライト行列（View×正射影）
// 旧シャドウマップ（20x20固定）でも、CSMの各段でも、同じこのシェーダーを使い回す。
// 「どの1枚を描いているか」はC++側がb8の中身を差し替えることで表現する
cbuffer ShadowPassBuffer : register(b8)
{
    matrix lightViewProj;
};

float4 VS(float4 pos : POSITION) : SV_POSITION
{
    float4 worldPos = mul(pos, mModel);
    return mul(worldPos, lightViewProj);
}

// ダミーPS（コンパイルエラー回避用）
float4 PS() : SV_Target
{
    return float4(0, 0, 0, 0);
}
