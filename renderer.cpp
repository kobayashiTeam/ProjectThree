#include "renderer.h"
#include "graphics.h"
#include "Camera.h"
#include "model.h"
#include "renderQueue.h"
#include "depthStencilStates.h"
#include "blendStates.h"
#include "mathUtils.h" // ComputeDistance 用
#include <DirectXMath.h>
#include"mesh.h"
#include"screenBlitPostProcess.h"

Renderer::~Renderer()
{
    // 所有権を持つマネジメントクラスの解放
    delete m_rasterStates;
    delete m_dsStates;
    delete m_blendStates;
    //delete m_renderQueue;
}

bool Renderer::Initialize(Graphics* graphics)
{
    if (!graphics) return false;
    m_graphics = graphics;
    ID3D11Device* pDevice = m_graphics->GetDevice();

    // 1. 各種ステートクラスの生成と初期化
    m_rasterStates = new RasterizerStates();
    if (!m_rasterStates->Initialize(pDevice)) return false;

    m_dsStates = new DepthStencilStates();
    if (!m_dsStates->Initialize(pDevice)) return false;

    m_blendStates = new BlendStates();
    if (!m_blendStates->Initialize(pDevice)) return false;

    //m_renderQueue = new RenderQueue();
    

    // 2. 定数バッファの作成
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerFrameCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, m_perFrameCB.GetAddressOf());
    if (FAILED(hr)) return false;

	// 3. オフスクリーンレンダーターゲットの初期化（テスト）
	m_offscreenRT = new RenderTarget();
	if (!m_offscreenRT->Initialize(pDevice, 1280, 720)) {
		return false;
	}

    return true;
}

void Renderer::BeginFrame(Camera* camera, float r, float g, float b, float a)
{
    m_currentCamera = camera;
    m_graphics->BeginScene(r, g, b, a);

    // 共通のトポロジー設定
    m_graphics->GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // フレームごとの定数バッファ更新
    UpdatePerFrameConstantBuffer();
}

void Renderer::UpdatePerFrameConstantBuffer()
{
    if (!m_currentCamera) return;

    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    PerFrameCB frameParams;
    frameParams.matView = DirectX::XMMatrixTranspose(m_currentCamera->GetViewMatrix());
    frameParams.matProjection = DirectX::XMMatrixTranspose(m_currentCamera->GetProjectionMatrix());
    DirectX::XMStoreFloat4(&frameParams.vLightPos, DirectX::XMVectorSet(0.0f, 3.0f, 0.0f, 1.0f));
    frameParams.vLightColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    frameParams.vEyePos = m_currentCamera->GetEyePosition();
    frameParams.vAttenuation = DirectX::XMFLOAT4(1.0f, 0.09f, 0.032f, 0.0f);

    pContext->UpdateSubresource(m_perFrameCB.Get(), 0, nullptr, &frameParams, 0, 0);

    // スロット0にバインド
    ID3D11Buffer* cbArray[] = { m_perFrameCB.Get() };
    pContext->VSSetConstantBuffers(0, 1, cbArray);
}

void Renderer::Submit(Model* model, RenderPass pass, BlendMode mode)
{
    if (!model || !m_currentCamera) return;

    // 距離計算
    DirectX::XMFLOAT4 camPos = m_currentCamera->GetEyePosition();
    DirectX::XMFLOAT3 modelPos = model->GetPosition();
    float depth = MyEngine::ComputeDistance(DirectX::XMLoadFloat4(&camPos), DirectX::XMLoadFloat3(&modelPos));

    // パスのインデックスを取得
    int passIdx = static_cast<int>(pass);

    // ★ if文で分岐しなくても、すべてのパスで共通の処理に一元化できます！
    if (passIdx >= 0 && passIdx < static_cast<int>(RenderPass::Count))
    {
        // 引数で入ってきた mode をそのままQueueのSubmitに渡す
        m_renderQueues[passIdx].Submit(model, depth, mode);
    }
}

void Renderer::Execute()
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    // ==========================================
    // 【新設】1. 描画先を「自作の裏画面」に切り替える（Offscreen Pass 開始）
    // ==========================================
    m_offscreenRT->Clear(pContext);
    m_offscreenRT->Bind(pContext); // ※前回統合した自作のレンダーターゲット

	//2. 各パスのキューを、適切なステートをセットしてから実行する

    int opaqueIdx = static_cast<int>(RenderPass::Opaque);
    int outlineIdx = static_cast<int>(RenderPass::Outline);
    int transparentIdx = static_cast<int>(RenderPass::Transparent);

    // ─── 工程1: 不透明パス ───
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); // 通常の深度テスト
    m_renderQueues[opaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates);

    // ─── 工程2: アウトラインパス ───
    //if (!m_renderQueues[outlineIdx].IsEmpty()) { // ※IsEmptyメソッドがあると便利
    //    this->BeginStencilOutlinePass(); // ステンシル等の特殊ステートON

    //    // アウトラインパスのキューを実行
    //    m_renderQueues[outlineIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates);

    //    this->EndStencilOutlinePass();  // ステートを戻す
    //}

    // ─── 工程3: 半透明パス ───
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); // 必要ならデプス書き込みOFFのステートなど
    m_renderQueues[transparentIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates);

    // ==========================================
    // 【新設】3. 出力先を「デフォルト（画面）」に戻してポストプロセス適用
    // ==========================================
    m_graphics->bindDefaultRenderTarget(); // 本物の画面をセット＋クリア
    // ★重要：最終描画はブレンドを「OFF（Opaqueモード）」にする！
    // 画面全体に上書きするだけなので、これ以前のAlpha値を完全に無視させます。
    m_blendStates->Bind(pContext, BlendMode::Opaque);
    //matにも規定クラスにsrvがあるが、ここではrendererが持っているsrvを
    //contextから設定している。ここに本来material用の派生クラスとしての煩雑さがある
    //m_finalRenderMat->BindScreenBlit(pContext,m_offscreenRT);
    m_finalRenderScreenBlitPostProcess->Render(pContext,m_offscreenRT);
    m_finalRenderMesh->Render(pContext);

}

void Renderer::EndFrame()
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    // 後処理：デフォルトのステンシルステートなどに戻す
    m_dsStates->Bind(pContext,DepthStencilStates::Mode::DepthTest);

    m_graphics->EndScene();
    m_currentCamera = nullptr;
}

void Renderer::SetCullMode(RasterizerStates::CullMode mode)
{
    m_rasterStates->Bind(m_graphics->GetContext(), mode);
}

void Renderer::BeginStencilOutlinePass()
{
    // 将来的にステンシルマスクを有効化するステート変更をここに記述
}

void Renderer::EndStencilOutlinePass()
{
    // ステンシルマスクを元に戻す処理をここに記述
}

bool Renderer::createFinalRenderQuad(Shader* screenBlitShader) {
    
    ID3D11Device* pDevice = m_graphics->GetDevice();
    if (!pDevice)return false;
    m_finalRenderMesh = Mesh::CreateQuad(pDevice);//mesh
    if (!m_finalRenderMesh)return false;

    /*m_finalRenderQuad = new Model(pDevice,m_finalRenderMesh,m_finalRenderMat);
    if (!m_finalRenderQuad)return false;*/

    //test:postprocess
    m_finalRenderScreenBlitPostProcess = new ScreenBlitPostProcess();
    m_finalRenderScreenBlitPostProcess->Initialize(pDevice,screenBlitShader);

    return true;
}