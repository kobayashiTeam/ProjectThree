// 1. C++側と完全に一致させる定数バッファ
cbuffer ConstantBuffer : register(b0)
{
    matrix mModel;
    matrix mView;
    matrix mProjection;
    float4 vLightDir; // ライトの方向
    float4 vLightColor; // ライトの色
};

// 2. 頂点シェーダーへの入力構造体 (インプットレイアウトに対応)
struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL; // ★追加
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

// 3. ピクセルシェーダーへのバトンタッチ用構造体
struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 wNormal : NORMAL; // ★追加：ワールド空間での法線
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD0;
};

// テクスチャとサンプラー
Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

// ------------------------------------------------------------------------
// 頂点シェーダー
// ------------------------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // 座標変換 (Local -> World -> View -> Projection)
    float4 worldPos = mul(input.Pos, mModel);
    float4 viewPos = mul(worldPos, mView);
    output.Pos = mul(viewPos, mProjection);
    
    // ★法線のワールド変換
    // 本来はモデル行列の逆転置行列をかけますが、拡大縮小（スケーリング）がない場合は
    // モデル行列をそのままかけて正規化するだけでワールド空間の向きになります。
    output.wNormal = normalize(mul(float4(input.Normal, 0.0f), mModel).xyz);
    
    output.Color = input.Color;
    output.TexCoord = input.TexCoord;
    
    return output;
}

// ------------------------------------------------------------------------
// ピクセルシェーダー
// ------------------------------------------------------------------------
float4 PS(PS_INPUT input) : SV_TARGET
{
    // A. テクスチャの色をサンプリング
    float4 texColor = txDiffuse.Sample(samLinear, input.TexCoord);
    
    // B. 環境光（Ambient）の計算
    // 光が全く当たっていない影の部分も、うっすら見えるようにする（例: 20%の明るさ）
    float3 ambient = float3(0.2f, 0.2f, 0.2f) * vLightColor.xyz;
    
    // C. 拡散反射（Diffuse）の計算
    // ランバートの余弦則：面の向き(Normal) と 光の届く方向 の内積（dot）を計算する
    // ※vLightDirは「ライトが進む向き」なので、計算時はマイナスを反転して「ライトへ向かう向き」にします。
    float3 lightVec = -vLightDir.xyz;
    float diffuseFactor = max(dot(input.wNormal, lightVec), 0.0f); // 負の数は0にする(max)
    float3 diffuse = diffuseFactor * vLightColor.xyz;
    
    // D. 最終的な光の強さをテクスチャ色に掛け算する
    float3 finalColor = (ambient + diffuse) * texColor.xyz;
    
    // アルファ値（透明度）はテクスチャのものをそのまま使う
    return float4(finalColor, texColor.a);
}