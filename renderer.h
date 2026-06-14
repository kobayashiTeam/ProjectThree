#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "rasterizerStates.h"
#include "graphicsCommon.h"

#pragma comment(lib, "d3d11.lib")
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class Graphics;
class Camera;
class Model;
class DepthStencilStates;
class BlendStates;
class RenderQueue;

class Renderer
{
public:
    enum class RenderPass
    {
        Opaque,
        Transparent,
        Outline,
    };
    //OutLineはどうしよう？

    Renderer() = default;
    ~Renderer();

    bool Initialize(Graphics* graphics);

    void BeginFrame(Camera* camera, float r, float g, float b, float a);
    void EndFrame();

    // 距離計算を含めてモデルを適切なパスに登録する
    void Submit(Model* model, RenderPass pass);
    void Execute();

    // ステート制御
    void SetCullMode(RasterizerStates::CullMode mode);
    void BeginStencilOutlinePass();
    void EndStencilOutlinePass();

private:
    void UpdatePerFrameConstantBuffer();

private:
    Graphics* m_graphics = nullptr;
    Camera* m_currentCamera = nullptr; // パラメータ更新や距離計算用に保持

    // メンバを独自ポインタからComPtrや適切なクラスポインタで管理
    RasterizerStates* m_rasterStates = nullptr;
    DepthStencilStates* m_dsStates = nullptr;
    BlendStates* m_blendStates = nullptr;
    RenderQueue* m_renderQueue = nullptr;

    ComPtr<ID3D11Buffer> m_perFrameCB;
};