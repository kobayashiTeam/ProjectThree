// =========================================================================
// SkyBoxShader.hlsl (VS / PS 一体型)
// =========================================================================

// スロット0：フレーム単位で共通（カメラやライトの情報）
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

// 頂点シェーダーに送る定数バッファ (スロット3)
cbuffer PerObjectBuffer : register(b3)
{
    matrix g_ViewProjection; // 平行移動成分を除去した View行列 × Projection行列
};

// 入力頂点構造（位置情報のみ）
struct VS_INPUT
{
    float3 position : POSITION;
};

// 頂点シェーダーからピクセルシェーダーへの出力構造
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD0; // 3次元サンプリングベクトル
};

// キューブマップテクスチャとサンプラーの設定
TextureCube g_SkyboxTexture : register(t0);
SamplerState g_SamplerLinear : register(s0);

// =========================================================================
// 頂点シェーダー (エントリーポイント: VS)
// =========================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;
    
    // 頂点位置を変換
    output.position = mul(float4(input.position, 1.0f), g_ViewProjection);
    
    // 深度値を強制的に最奥(1.0)にするトリック
    // ラスタライズ後の z/w が 1.0 になるよう、z に w を代入する
    output.position.z = output.position.w;
    
    // 立方体のローカル座標をそのままキューブマップのサンプリングベクトルとして使用
    output.texCoord = input.position;
    
    return output;
}

// =========================================================================
// ピクセルシェーダー (エントリーポイント: PS)
// =========================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    // 3次元の方向ベクトルを用いてキューブマップから色をサンプリング
    return g_SkyboxTexture.Sample(g_SamplerLinear, input.texCoord);
    return float4(1.0f,0.0f,0.0f,1.0f);

}