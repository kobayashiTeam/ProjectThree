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
    Depth
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
    HoriBlur,
    VerBlur,
    BloomCombine,
    //skybox
    SkyBox,
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
    GBufferDebugDepth //Depth専用：線形化＋グレースケール化
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
    DirectX:: XMMATRIX gLightViewProj[6]; // GSが使う
    DirectX::XMFLOAT3 gLightPos;
    float gFarPlane;
};

//ポストプロセス用のcb
struct PostProcessConstantBuffer {
    float exposure = 1.0f;
    float gammaCorrection = 1.0f; // Scene2用：1.0=ON（補正あり）、0.0=OFF（補正なし）
    float padding[2] = { 0.0f, 0.0f }; // 16バイトアライメント
};

// SSAOパラメータ用構造体 (16バイトアライメントを保証)
struct SSAOParam
{
    DirectX::XMFLOAT4 samples[64]; // 16バイト * 64 = 1024バイト
    DirectX::XMFLOAT2 noiseScale;  // 8バイト
    float             radius;      // 4バイト
    float             bias;        // 4バイト  (合計 16バイト)
}; // 全体で 1040 バイト（16バイトアライメント要件を満たす）