#pragma once
#include <DirectXMath.h>

// シェーダーの Constant Buffer (b0) の物理的なレイアウトに完全一致させる構造体
struct PerFrameCB
{
    DirectX::XMMATRIX matView;
    DirectX::XMMATRIX matProjection;
    DirectX::XMFLOAT4 vLightPos;
    DirectX::XMFLOAT4 vLightColor;
    DirectX::XMFLOAT4 vEyePos;
    DirectX::XMFLOAT4 vAttenuation;
};

//ブレンドタイプの共通参照enum
enum class BlendMode
{
    Opaque,       // 不透明（None）
    AlphaBlend,   // 半透明
    Additive,     // 加算合成（エフェクト用）
    Count         // バッファの数（自動的に 3 になる）
};

// 描画の工程（ゲームのレンダリングステップ）
enum class RenderPass {
    Opaque,       // 通常の不透明オブジェクト
    Transparent,  // 半透明オブジェクト
    DeferredOpaque, // Lit系シェーダーを使うオブジェクトはこちらに登録
    Count
};

// Scene3：Gバッファのどの成分を最終出力するか（Renderer側でこの状態を保持する設計）
enum class GBufferDebugView {
    Lit,     // 通常通り、Lighting済みの最終結果
    Albedo,
    Normal,
    Depth,
    Metallic,  //  追加：Albedo.aの生データをグレースケール表示（PBRデータ検証用）
    Roughness  //  追加：Normal.aの生データをグレースケール表示（PBRデータ検証用）
};

// Scene5：シャドウマッピングでどちらの光源をライティングに反映するか
enum class LightVisibilityMode {
    DirectionalOnly,
    PointOnly,
    Both
};

enum class ShaderID {
    Lit,
    LitInstancing,
    UnLit,
    ScreenBlit,
    NormalViz,
    PointSprite,
    NormalMapping,
    ParallaxMapping,
    BasicColor,
    // postProcess
    Monochromatic,
    Inversion,
    Sepia,
    SimpleBoxBlur,
    Sharpen,
    Vignette,
    // Bloom（縮小／拡大の連鎖）とトーンマッピング
    BloomDownsample,
    BloomUpsample,
    Tonemap,
    //skybox
    SkyBox,
    //IBL
    IrradianceConvolution,
    PrefilterSpecular,
    Count,
    //geometry
    NormalVizGS,
    PassThrough,
    Move,
    PointSpriteGS,
    ShadowCubeGS,
    //shadow
    Shadow,
    ShadowCube,
    //deferred
    DeferredGB,
    DeferredLighting,
    //SSAO
    SSAO,
    SSAOBlur,
    //Scene3デバッグ表示用（Gバッファをそのままブリット）
    GBufferDebug,
    GBufferDebugDepth, //Depth専用：線形化＋グレースケール化
    GBufferDebugAlpha  //Scene9用。アルファチャンネル(metallic/roughness)をグレースケール表示
};

//ライト関連
#define MAX_LIGHTS 4

enum class LightType {
    Directional,
    Point,
    Spot
};


// GPU側に送る1ライト分のデータ（type フィールドで Directional/Point/Spot を判別）
struct LightData  // GPU側に送る1ライト分のデータ
{
    DirectX::XMFLOAT4 position;         // w未使用
    DirectX::XMFLOAT4 direction;        // w未使用
    DirectX::XMFLOAT4 color;
    DirectX::XMMATRIX lightSpaceMatrix; // シャドウ用
    int   type;
    float intensity;
    float farPlane;
    float padding;
};

// 全ライトをまとめてb3に送るバッファ
struct LightBufferCB  //b3CBに送る内容。LightData内容（上記）はシェーダ側で再定義する
{
    LightData lights[MAX_LIGHTS];
    int       lightCount;
    float     padding[3];
};


//Point ライト
struct ShadowCubeCB//b4CBに送る内容。内容が一個（view*Proj行列）しかないのでむき出しで送る
{
    DirectX::XMMATRIX gLightViewProj[6]; // GSが使う
    DirectX::XMFLOAT3 gLightPos;
    float gFarPlane;
};

// ===== カスケードシャドウマップ（CSM） =====
#define NUM_CASCADES 3
#define CASCADE_SHADOW_MAP_SIZE 2048

// シャドウマップ描画パス（VS）用：「今描いている1枚」のライト行列だけを渡すb8
// 旧実装はShadowShaderがb3のlights[0]を決め打ちで読んでいたため、
// PointOnlyモードでlights[0]が点光源になるとゼロ行列で描画される潜在バグがあった
struct ShadowPassCB
{
    DirectX::XMMATRIX lightViewProj;
};

// ライティングパス（PS）用：全カスケードの行列と境界深度を渡すb7
struct CascadeShadowCB
{
    DirectX::XMMATRIX cascadeViewProj[NUM_CASCADES]; // 各段のライトView×正射影
    DirectX::XMFLOAT4 cascadeSplits;  // x,y,z：各段の「奥側の境界」（カメラのビュー空間深度）
    DirectX::XMFLOAT4 cascadeBias;    // x,y,z：各段の深度バイアス（段ごとに1テクセルの大きさが違うため）
    int   cascadeEnabled;             // 0ならライティングは旧20x20シャドウマップを使う
    int   cascadeDebug;               // 1なら段ごとに色を付けて表示する
    float cascadeTexelSize;           // PCF用：1テクセルのUV幅（1/2048）
    float padding;
};

// トーンマッピングの方式（Scene6で切り替えて比較する）
enum class Tonemapper {
    Exposure, // 1 - exp(-x * exposure)。従来の方式
    ACES      // ACES Filmic（Narkowiczの近似式）。暗部が締まり、明部が粘る映画的なカーブ
};

//ポストプロセス用のcb（b5）。Tonemap.hlsl と ScreenBlit.hlsl の両方が読む
struct PostProcessConstantBuffer {
    float exposure = 1.0f;
    float gammaCorrection = 1.0f; // Scene2用：1.0=ON（補正あり）、0.0=OFF（補正なし）
    float tonemapper = 0.0f;      // 0.0=Exposure、1.0=ACES（HLSL側で0.5を境に分岐）
    float bloomIntensity = 0.0f;  // Bloomを足す強さ。Bloom OFFのときは0を送る
};

// Bloomの縮小／拡大パス用のcb（b9）
struct BloomParamsCB {
    DirectX::XMFLOAT2 srcTexelSize; // 読み込み元テクスチャの1テクセルのUV幅
    float threshold;                // 輝度のしきい値（これを超えた分がにじむ）
    float knee;                     // しきい値の手前から効き始める幅（ソフトニー）
    float isFirstPass;              // 1.0なら最初の縮小（しきい値処理＋Karis平均を行う）
    float filterRadius;             // 拡大時のテントフィルタの広がり（テクセル単位の倍率）
    float padding[2];
};

// SSAOパラメータ用構造体 (16バイトアライメントを保証)
struct SSAOParam
{
    DirectX::XMFLOAT4 samples[64]; // 16バイト * 64 = 1024バイト
    DirectX::XMFLOAT2 noiseScale;  // 8バイト
    float             radius;      // 4バイト
    float             bias;        // 4バイト  (合計 16バイト)
}; // 全体で 1040 バイト（16バイトアライメント要件を満たす）
