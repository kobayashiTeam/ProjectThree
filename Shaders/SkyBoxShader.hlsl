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

// 頂点シェーダーに送る定数バッファ (スロット1)
cbuffer PerObjectBuffer : register(b3)
{
    matrix g_ViewProjection; // ★平行移動成分を除去した View行列 × Projection行列
};

// 入力頂点構造（位置情報のみ）
struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL; // 追加（使わないが宣言だけする）
    float4 color : COLOR; // 追加
    float2 texcoord : TEXCOORD0; // 追加
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
// 頂点シェーダー (エントリーポイント: VS_Main)
// =========================================================================
VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // 立方体のローカル頂点座標を、そのままキューブマップのサンプリングベクトルとして使用
    //output.texCoord = normalize(input.position);
    output.texCoord = input.position;
    
    // 座標を変換 (w = 1.0 として扱う)
    float4 pos = mul(g_ViewProjection, float4(input.position, 1.0f));
    
    // ★【パースペクティブ・トリック】
    // Z成分をW成分に置き換える。これにより画面空間へ変換された際、
    // 深度(Z/W)が必ず「1.0」(もっとも遠い奥) になる。
    //output.position = pos.xyww;
    //正しい
    output.position = pos;
// そして深度を最大にしたいなら
    output.position.z = output.position.w;
    
    return output;
}

// =========================================================================
// ピクセルシェーダー (エントリーポイント: PS_Main)
// =========================================================================
float4 PS(VS_OUTPUT input) : SV_TARGET
{
    // texCoordの値を色として表示して確認する
    //return float4(input.texCoord * 0.5 + 0.5, 1.0);
    // 3次元の方向ベクトルを用いてキューブマップから色をサンプリング
    return g_SkyboxTexture.Sample(g_SamplerLinear, input.texCoord);
    return float4(1.0f,0.0f,0.0f,1.0f);

}