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

class Renderer
{
public:

    Renderer() = default;
    ~Renderer();

    bool Initialize(Graphics* graphics);

    void BeginFrame(Camera* camera, float r, float g, float b, float a);
    void EndFrame();

    // 距離計算を含めてモデルを適切なパスに登録する
    void Submit(Model* model, RenderPass pass,BlendMode mode);
    void Execute();

    // ステート制御
    void SetCullMode(RasterizerStates::CullMode mode);
    void BeginStencilOutlinePass();
    void EndStencilOutlinePass();

    //test
    bool createFinalRenderQuad();

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

    //テスト：オフスクリーンレンダーターゲット
	RenderTarget* m_offscreenRT=nullptr;
    RenderTarget* m_tmpRT = nullptr;//ピンポン設計にするためにもう一枚
    // ★【核心】このフレームで「実行する予定の全エフェクト」を並べるコンテナ
    std::vector<PostProcess*> m_postProcessChain;


    //ポストプロセス後に描画するQuadのmodel
    Model* m_finalRenderQuad = nullptr;
    ScreenBlitMaterial* m_finalRenderMat = nullptr;
    Mesh* m_finalRenderMesh = nullptr;
    Shader* m_finalRenderSahder = nullptr;
    //test:postProcessクラス
    ScreenBlitPostProcess* m_finalRenderScreenBlitPostProcess = nullptr;//simpleBlit
    MonochromePostProcess* m_finalRenderMonochromePostProcess = nullptr;//monochrome
    InversionPostProcess* m_finalRenderInversionPostProcess = nullptr;//inversion


};