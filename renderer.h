#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "rasterizerStates.h"
#include "graphicsCommon.h"
#include"renderTarget.h"

#pragma comment(lib, "d3d11.lib")
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

//実体宣言
#include"renderQueue.h"
#include"light.h"
#include"shadowMap.h"
#include"shadowCubeMap.h"

//前方宣言
class Graphics;
class Camera;
class Model;
class DepthStencilStates;
class BlendStates;
class ScreenBlitMaterial;
class ScreenBlitPostProcess;
class MonochromePostProcess;
class PostProcess;
class InversionPostProcess;
class SepiaPostProcess;
class SimpleBoxBlurPostProcess;
class SharpenPostProcess;
class VignettePostProcess;
class HorizontalBlurPostProcess;
class VerticalBlurPostProcess;
class SkyBox;
class PointSpriteGSEffect;
class InstancedModel;
class BloomCombinePostProcess;
class PostProcessChain;
class BloomBlurPass;
class GBufferPass;
class GBufferDebugBlit;
class SSAOPass;
class ShadowSystem;
class DeferredLightingPass;

class Renderer
{
public:

    Renderer() = default;
    ~Renderer();

    bool Initialize(Graphics* graphics);

    void BeginFrame(Camera* camera, float r, float g, float b, float a);
    void EndFrame();

    // 距離計算を含めてモデルを適切なパスに登録する
    void Submit(Model* model, RenderPass pass, BlendMode mode);
    void Execute();

    // ステート制御
    void SetCullMode(RasterizerStates::CullMode mode);

    //defaultRTに戻してから描画するQuadオブジェクト
    bool createFinalRenderQuad();

    //ポストプロセスバッファ
    void UpdatePostProcessConstantBuffer();
    void SetExposure(float exposure) { m_postProcessData.exposure = exposure; }
    float GetExposure() const { return m_postProcessData.exposure; }
    // Bloom（BloomCombinePostProcess）のON/OFF切り替え
    void SetBloomActive(bool isOn);
    bool IsBloomActive() const;
    // ガンマ補正のON/OFF切り替え（ScreenBlitパスのpow(1/2.2)を分岐）
    void SetGammaCorrection(bool isOn) { m_postProcessData.gammaCorrection = isOn ? 1.0f : 0.0f; }

    // Gバッファのデバッグ表示モード切り替え（案A）
    void SetDebugView(GBufferDebugView view) { m_debugView = view; }
    // DirectionalLightの向きを変更（ShadowSystemへ委譲）
    void SetDirectionalLightDirection(DirectX::XMFLOAT3 dir);
    // PointLightの位置を変更（ShadowSystemへ委譲）
    void SetPointLightPosition(DirectX::XMFLOAT3 pos);
    // Directional/Point/Bothの表示切り替え（ShadowSystemへ委譲）
    void SetLightVisibilityMode(LightVisibilityMode mode);
    // GPUインスタンシングの表示個数を変更（InstancedModelへ委譲）
    void SetInstanceCount(UINT count);
    // 表示個数の変更に加え、群全体をオフセット分だけ移動させる
    void SetInstanceCount(UINT count, DirectX::XMFLOAT3 offset);


private:
    void UpdatePerFrameConstantBuffer();

private:
    Graphics* m_graphics = nullptr;
    Camera* m_currentCamera = nullptr; // パラメータ更新や距離計算用に保持

    // メンバを独自ポインタからComPtrや適切なクラスポインタで管理
    RasterizerStates* m_rasterStates = nullptr;
    DepthStencilStates* m_dsStates = nullptr;
    BlendStates* m_blendStates = nullptr;
    // レンダーパスの数だけ、個別のRenderQueueを持つ
    RenderQueue m_renderQueues[static_cast<int>(RenderPass::Count)];

    ComPtr<ID3D11Buffer> m_perFrameCB;

    //オフスクリーンレンダーターゲット
    RenderTarget* m_offscreenRT = nullptr;
    RenderTarget* m_offscreenRTwithMSAA = nullptr;// MSAA用オフスクリーンRT
    RenderTarget* m_tmpRT = nullptr;//ピンポン設計にするためにもう一枚
    // bloom対応のrt
    RenderTarget* m_brightRTwithMSAA = nullptr;
    RenderTarget* m_brightRT = nullptr;


    //ポストプロセス後に描画するQuadのmodel
    ScreenBlitMaterial* m_finalRenderMat = nullptr;
    Mesh* m_finalRenderMesh = nullptr;
    Shader* m_finalRenderShader = nullptr;
    // 最終描画用ポストプロセス
    ScreenBlitPostProcess* m_finalRenderScreenBlitPostProcess = nullptr;

    // Bloom合成用ポストプロセス
    BloomCombinePostProcess* m_finalRenderBloomCombinePostProcess = nullptr;
    // postprocessを担当するクラス
    PostProcessChain* m_postProcessChain = nullptr;
    //例外的なbloomBlurは専門パスとして別クラスに
    BloomBlurPass* m_bloomBlurPass = nullptr;

    //スカイボックスオブジェクト
    SkyBox* m_pSkyBox = nullptr;
    //pointSprite
    PointSpriteGSEffect* m_pPointSpriteGSEffect = nullptr;
    //instancedModel
    InstancedModel* m_pInstancedModel = nullptr;


    //ポストプロセスバッファ、参照
    ComPtr<ID3D11Buffer> m_pPostProcessCB = nullptr;
    PostProcessConstantBuffer m_postProcessData;

    // Deferred Rendering用G-Buffer関連リソース
        //シェーダ
    Shader* m_pDeferredGBufferShader = nullptr;
    Shader* m_pDeferredLightingShader = nullptr;
    //サンプラー
    ComPtr<ID3D11SamplerState> m_gBufferDepthSampler;


    // Gbuffer
    GBufferPass* m_gBufferPass = nullptr;
    // DeferredLightingPass
    DeferredLightingPass* m_deferredLightingPass = nullptr;
    // SSAOPass
    SSAOPass* m_ssaoPass = nullptr;
    // ShadowSystem
    ShadowSystem* m_shadowSystem = nullptr;

    // Scene3のGバッファデバッグ表示（案A）
    GBufferDebugView m_debugView = GBufferDebugView::Lit;
    GBufferDebugBlit* m_gBufferDebugBlit = nullptr;
    // Scene9用：Albedo.a/Normal.a（metallic/roughness）をグレースケール表示する専用ブリット
    GBufferDebugBlit* m_gBufferDebugAlphaBlit = nullptr;

};