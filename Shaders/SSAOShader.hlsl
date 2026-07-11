// =========================================================
// SSAO (Screen Space Ambient Occlusion) 生成シェーダー
// G-Buffer(ワールド空間)を読み込み、View空間に変換して計算します。
// =========================================================

// ---------------------------------------------------------
// 定数バッファ
// ---------------------------------------------------------
// ---------------------------------------------------------
// 定数バッファの分割
// ---------------------------------------------------------
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

cbuffer PerObjectBuffer : register(b1)
{
    matrix mModel;
};

cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor;
};

struct LightData
{
    float4 position;
    float4 direction;
    float4 color;
    matrix lightSpaceMatrix;
    int type;
    float intensity;
    float farPlane;
    float padding;
};

cbuffer LightBuffer : register(b3)
{
    LightData lights[4];
    int lightCount;
    float3 padding;
};

// =========================================================
// 【新設】SSAO専用のデータを空きスロット (b6) に配置！
// =========================================================
cbuffer SSAOParamBuffer : register(b6)
{
    // samples や radius など、SSAOだけで使う調整パラメータをまとめます
    float4 samples[64];
    float2 noiseScale;
    float radius;
    float bias;
};
// ---------------------------------------------------------
// リソース (テクスチャとサンプラー)
// ---------------------------------------------------------
// G-Bufferは前段のパスでワールド空間として書き込まれたものを想定
Texture2D txNormal : register(t0); // G-Buffer: ワールド空間の法線
Texture2D txPosition : register(t1); // G-Buffer: ワールド空間の座標
Texture2D txNoise : register(t2); // 4x4のランダムベクトル(回転用)

// G-Bufferはピクセル単位で正確に読みたいのでPointサンプリング、UV範囲外はClamp
SamplerState samPointClamp : register(s0);
// ノイズテクスチャは画面全体に敷き詰める（タイリングする）のでWrap設定が必要
SamplerState samPointWrap : register(s1);

// ---------------------------------------------------------
// 入出力構造体
// ---------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

// ---------------------------------------------------------
// 頂点シェーダー (フルスクリーンクアッド用)
// ---------------------------------------------------------
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;
    
    // ポストプロセスなので、入力された頂点座標をそのまま画面の座標(Clip Space)として扱う
    // (C++側の描画に使うQuadメッシュは x,y が -1.0 ～ 1.0 になっている前提)
    output.Pos = float4(input.Pos.x, input.Pos.y, 0.0f, 1.0f);
    output.Tex = input.Tex;
    
    return output;
}

// ---------------------------------------------------------
// ピクセルシェーダー
// ---------------------------------------------------------
float4 PS(PS_INPUT input) : SV_Target
{
    // 1. G-Bufferから情報を取得 (World Space)
    float3 worldPos = txPosition.Sample(samPointClamp, input.Tex).xyz;
    float3 worldNormal = txNormal.Sample(samPointClamp, input.Tex).xyz;
    
    // ※もし背景（モデルがない場所）ならSSAOは計算せず白(1.0)を返す
    // 深度値や、Normalがゼロベクトルかどうか等で判定できます（ここでは単純な0判定）
    if (length(worldNormal) < 0.1f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    // 2. World Space -> View Space への変換
    // 位置の変換（平行移動を含むため mul(float4(v, 1), m)）
    float3 viewPos = mul(float4(worldPos, 1.0f), mView).xyz;
    
    // 法線の変換（平行移動を無視するため 3x3 行列キャスト、またはW=0）
    float3 viewNormal = normalize(mul(float4(worldNormal, 0.0f), mView).xyz);
    
    // 3. ランダムな回転用ベクトルの取得 (タイリングさせるために noiseScale を掛ける)
    float3 randomVec = txNoise.Sample(samPointWrap, input.Tex * noiseScale).xyz;
    
    // 4. TBN行列の構築 (Tangent Space -> View Space)
    // グラム・シュミットの直交化法で接空間の基底ベクトルを作る
    float3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, viewNormal); // HLSLのfloat3x3生成
    
    // 5. SSAOの計算 (64個のサンプルをループ)
    float occlusion = 0.0f;
    int kernelSize = 64;
    
    for (int i = 0; i < kernelSize; ++i)
    {
        // サンプルポイントをTBN行列で回転させてView空間に持ってくる
        float3 samplePos = mul(samples[i].xyz, TBN); // Tangent -> View
        
        // 実際のView空間上のサンプル座標を決定
        samplePos = viewPos + samplePos * radius;
        
        // 6. サンプル座標を画面上のUV座標(Screen Space)に変換して、実際の深度を覗き見する
        float4 offset = float4(samplePos, 1.0f);
        offset = mul(offset, mProjection); // View -> Clip Space
        offset.xyz /= offset.w; // パースペクティブ除算 (NDC空間へ: -1.0 ～ 1.0)
        
        // NDC空間 (-1.0 ～ 1.0) を UV空間 (0.0 ～ 1.0) に変換
        // ※DirectXはY軸が下向きなので反転させる (-0.5)
        float2 sampleUV = offset.xy * float2(0.5f, -0.5f) + 0.5f;
        
        // 7. サンプルしたUVの位置にある「実際のワールド座標」を取得し、View空間のZ深度に変換
        float3 sampleWorldPos = txPosition.SampleLevel(samPointClamp, sampleUV, 0).xyz;
        float sampleDepth = mul(float4(sampleWorldPos, 1.0f), mView).z;
        
        // 8. 遮蔽判定
        // レンジチェック：手前にある物体が遠くの物体に間違って影を落とさないための処理
        // (abs(viewPos.z - sampleDepth) < radius) ならば、判定を有効にする
        float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(viewPos.z - sampleDepth));
        
        // サンプル点のZが、実際のZよりも奥(カメラから遠い＝値が大きい)なら遮蔽されている
        // ※DirectXのView空間（右手/左手）によりますが、一般的にカメラ前方はZ値が大きい(または小さい)。
        // 左手系(Zがプラスに伸びる)の場合： sampleDepth < samplePos.z なら遮蔽。
        if (sampleDepth <= samplePos.z - bias)
        {
            occlusion += 1.0f * rangeCheck;
        }
    }
    
    // 9. 最終的なオクルージョン値の算出 (0.0=真っ暗, 1.0=遮蔽なし)
    occlusion = 1.0f - (occlusion / (float) kernelSize);
    
    // Rチャンネルのみを使用しますが、結果を見やすくするためにfloat4で出力
    return float4(occlusion, occlusion, occlusion, 1.0f);
}