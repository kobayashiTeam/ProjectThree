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
#include"gBufferDebugBlit.h"
#include"shaderManager.h"
#include"monochromePostProcess.h"
#include"inversionPostProcess.h"
#include"sepiaPostProcess.h"
#include"simpleBoxBlurPostProcess.h"
#include"sharpenPostProcess.h"
#include"vignettePostProcess.h"
#include"skyBox.h"
#include<string>
#include<array>
#include <filesystem>
#include"pointSpriteGSEffect.h"
#include"instancedModel.h"
#include"litMaterial.h"
#include"light.h"
#include"graphicsCommon.h"
#include"HorizontalBlurPostProcess.h"
#include"VerticalBlurPostProcess.h"
#include"bloomCombinePostProcess.h"
#include<random>
#include"postProcessChain.h"
#include"bloomBlurPass.h"
#include"gBufferPass.h"
#include"ssaoPass.h"
#include"shadowSystem.h"
#include"deferredLightingPass.h"

Renderer::~Renderer()
{
    // 所有権を持つマネジメントクラスの解放
    delete m_rasterStates;
    delete m_dsStates;
    delete m_blendStates;
}

bool Renderer::Initialize(Graphics* graphics)
{
    if (!graphics) return false;
    m_graphics = graphics;
    ID3D11Device* pDevice = m_graphics->GetDevice();
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    // 1. 各種ステートクラスの生成と初期化
    m_rasterStates = new RasterizerStates();
    if (!m_rasterStates->Initialize(pDevice)) return false;

    m_dsStates = new DepthStencilStates();
    if (!m_dsStates->Initialize(pDevice)) return false;

    m_blendStates = new BlendStates();
    if (!m_blendStates->Initialize(pDevice)) return false;


    // 2. 定数バッファの作成
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerFrameCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, m_perFrameCB.GetAddressOf());
    if (FAILED(hr)) return false;

	// 3. 「オフスクリーンRTの初期化（HDR対応のため R16G16B16A16_FLOAT）」
	m_offscreenRT = new RenderTarget();
	if (!m_offscreenRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
		return false;
	}

    m_offscreenRTwithMSAA = new RenderTarget();
    if (!m_offscreenRTwithMSAA->InitializeWithMSAA(
        pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }

        //オフスクリーンレンダー２号の初期化（swapChainで使用）
    m_tmpRT = new RenderTarget();
    if (!m_tmpRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }

	m_brightRT = new RenderTarget();
	if (!m_brightRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
		return false;
	}
	m_brightRTwithMSAA = new RenderTarget();
    if (!m_brightRTwithMSAA->InitializeWithMSAA(
        pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
	

    //simpleBlit
    m_finalRenderScreenBlitPostProcess = new ScreenBlitPostProcess();
    m_finalRenderScreenBlitPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::ScreenBlit));

    // Gバッファデバッグ表示用
    m_gBufferDebugBlit = new GBufferDebugBlit();
    m_gBufferDebugBlit->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::GBufferDebug));
 

    // PostProcessChain の生成・初期化
	m_postProcessChain = new PostProcessChain();
    m_postProcessChain->AddEffect<MonochromePostProcess>(pDevice, ShaderID::Monochromatic, false);
    m_postProcessChain->AddEffect<InversionPostProcess>(pDevice, ShaderID::Inversion, false);
    m_postProcessChain->AddEffect<SepiaPostProcess>(pDevice, ShaderID::Sepia, false);
    m_postProcessChain->AddEffect<SimpleBoxBlurPostProcess>(pDevice, ShaderID::SimpleBoxBlur, false);
    m_postProcessChain->AddEffect<SharpenPostProcess>(pDevice, ShaderID::Sharpen, false);
    m_postProcessChain->AddEffect<VignettePostProcess>(pDevice, ShaderID::Vignette, false);
    m_finalRenderBloomCombinePostProcess= m_postProcessChain->AddEffect<BloomCombinePostProcess>(pDevice, ShaderID::BloomCombine, true);

        // BloomBlur は別パスとして独立クラスで管理
	m_bloomBlurPass = new BloomBlurPass();
	m_bloomBlurPass->Initialize(pDevice, 1280, 720);

    //最終描画用のquadをここで生成
    if (!this->createFinalRenderQuad())return false;

    //スカイボックスの初期化
    m_pSkyBox = new SkyBox();
    std::array<std::wstring, 6> skyboxFaces = {
    L"assets/skybox/vz_dawn_right.png",  // [0] +X
    L"assets/skybox/vz_dawn_left.png",   // [1] -X
    L"assets/skybox/vz_dawn_up.png",     // [2] +Y
    L"assets/skybox/vz_dawn_down.png",   // [3] -Y
    L"assets/skybox/vz_dawn_front.png",  // [4] +Z
    L"assets/skybox/vz_dawn_back.png",   // [5] -Z
    };


    if (!m_pSkyBox->Initialize(pDevice, skyboxFaces)) {
        return false;
    }

    //点をポリゴンに変えるクラスの生成、初期化
    m_pPointSpriteGSEffect = new PointSpriteGSEffect();
    if (!m_pPointSpriteGSEffect->Init(pDevice))return false;

    // InstancedModel の初期化
    m_pInstancedModel = new InstancedModel();
    if (!m_pInstancedModel->Init(pDevice, pContext,Mesh::CreateCube(pDevice, 1.0f), 512))return false;
    

    // ShadowSystem の生成・初期化
	m_shadowSystem = new ShadowSystem();
	m_shadowSystem->Initialize(pDevice);


    //ポストプロセスバッファの初期化
        // b5用の定数バッファを作成
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC; // 毎フレーム更新できるようにDYNAMIC
    desc.ByteWidth = sizeof(PostProcessConstantBuffer); // 必ず16の倍数になる
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = pDevice->CreateBuffer(&desc, nullptr, &m_pPostProcessCB);
    if (FAILED(hr)) return false;
        
        //内容を初期設定
    SetExposure(0.5f);
    SetGammaCorrection(true); // デフォルトでガンマ補正を有効化

	//DeferredLightingPassの初期化
	m_deferredLightingPass = new DeferredLightingPass();
	m_deferredLightingPass->Initialize(pDevice);

	//GBufferPassの初期化
	m_gBufferPass = new GBufferPass();
	m_gBufferPass->Initialize(pDevice, 1280, 720);
    
    //SSAO
	m_ssaoPass = new SSAOPass();
	m_ssaoPass->Initialize(pDevice, 1280, 720);

    return true;

}

void Renderer::BeginFrame(Camera* camera, float r, float g, float b, float a)
{
	ID3D11DeviceContext* pContext = m_graphics->GetContext();

    m_currentCamera = camera;
    m_graphics->BeginScene(r, g, b, a);

    // 共通のトポロジー設定
    m_graphics->GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // フレームごとの定数バッファ更新(b0)
    UpdatePerFrameConstantBuffer();
        //DirectionalLightも共通なので送る(b3)
	m_shadowSystem->UpdateLightDataConstantBuffer(pContext);
        //PointLightも送る(b4)
    m_shadowSystem->UpdatePointLightConstantBuffer(pContext);
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
    //PSにもこれを設定
    pContext->PSSetConstantBuffers(0, 1, cbArray);
    //GSにも同cbを設定
    pContext->GSSetConstantBuffers(0,1,cbArray);
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

    if (passIdx >= 0 && passIdx < static_cast<int>(RenderPass::Count))
    {
        // 引数で入ってきた mode をそのままQueueのSubmitに渡す
        m_renderQueues[passIdx].Submit(model, depth, mode);
    }
}

void Renderer::Execute()
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    int opaqueIdx = static_cast<int>(RenderPass::Opaque);
    int transparentIdx = static_cast<int>(RenderPass::Transparent);
    int deferredOpaqueIdx = static_cast<int>(RenderPass::DeferredOpaque);

    // ===== 1パス目：シャドウマップ生成 =====
    // ===== DirectionalLight のシャドウパス =====
    m_shadowSystem->BeginDirectionalPass(pContext);  // 内部でm_shadowShader->Bind()済み
    m_renderQueues[opaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_renderQueues[deferredOpaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_shadowSystem->EndDirectionalPass(pContext);


    // ===== PointLight のシャドウパス =====
    m_shadowSystem->BeginPointPass(pContext);
    m_renderQueues[opaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_renderQueues[deferredOpaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_shadowSystem->EndPointPass(pContext);

    // ==========================================
    // 1. オフスクリーンRTに描画先を切り替え
    // ==========================================
    m_offscreenRTwithMSAA->Clear(pContext);
	m_brightRTwithMSAA->Clear(pContext);

    // 配列にして準備
    RenderTarget* targets[2] = {
        m_offscreenRTwithMSAA,
        m_brightRTwithMSAA
    };
    // 深度バッファは代表して1つ目のものから取得して渡す
    ID3D11DepthStencilView* dsv = m_offscreenRTwithMSAA->GetDSV();
    //複数RTを一括バインド
    RenderTarget::BindMultiple(pContext, 2, targets, dsv);


    //-----Lighting Pass-----
    m_shadowSystem->BindForLighting(pContext);  // シャドウマップ関連の設定

    //-----deferred不透明パス-----
	m_gBufferPass->Begin(pContext);             // G-Bufferのレンダーターゲットに切り替え

    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest);

    m_renderQueues[deferredOpaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates, true);

    // ==========================================
    // Scene3：Gバッファのデバッグ表示
    // この時点でG-Buffer（Albedo/Normal/Depth）は埋まっているので、
    // Lit以外のモードならSSAO/Lighting/ポストプロセスを全部飛ばして直接ブリットする
    // ==========================================
    if (m_debugView != GBufferDebugView::Lit)
    {
        ID3D11ShaderResourceView* debugSRV = nullptr;
        switch (m_debugView)
        {
        case GBufferDebugView::Albedo: debugSRV = m_gBufferPass->GetAlbedoSRV(); break;
        case GBufferDebugView::Normal: debugSRV = m_gBufferPass->GetNormalSRV(); break;
        case GBufferDebugView::Depth:  debugSRV = m_gBufferPass->GetDepthSRV();  break;
        default: break;
        }

        m_graphics->bindDefaultRenderTarget(); // 本物の画面をセット＋クリア
        m_blendStates->Bind(pContext, BlendMode::Opaque);

        // Depthだけは生の非線形値だと真っ赤に潰れて見えなくなるため、専用の線形化シェーダーを直接使う
        if (m_debugView == GBufferDebugView::Depth)
        {
            ShaderManager::GetInstance().GetShader(ShaderID::GBufferDebugDepth)->Bind(pContext);
            pContext->PSSetShaderResources(0, 1, &debugSRV);
            ID3D11SamplerState* depthSampler = m_gBufferPass->GetDepthSampler();
            pContext->PSSetSamplers(0, 1, &depthSampler);
        }
        else
        {
            m_gBufferDebugBlit->Render(pContext, debugSRV);
        }
        m_finalRenderMesh->Render(pContext);
        return; // 通常のSSAO/Lighting/ポストプロセスチェーンはスキップ
    }


    // ==========================================
    // SSAO 生成 ＆ SSAOブラー パス
    // ==========================================
    
    ID3D11ShaderResourceView* ssaoSRV =m_ssaoPass->Execute(
        pContext, 
        m_gBufferPass->GetNormalSRV(), 
        m_gBufferPass->GetPositionSRV(), 
        m_finalRenderMesh);

    ////ssaoSRVを視覚化する
    //m_graphics->bindDefaultRenderTarget(); // 本物の画面をセット＋クリア
    //m_blendStates->Bind(pContext, BlendMode::Opaque);
    //ShaderManager::GetInstance().GetShader(ShaderID::GBufferDebug)->Bind(pContext);
    //pContext->PSSetShaderResources(0, 1, &ssaoSRV);
    //ID3D11SamplerState* depthSampler = m_gBufferPass->GetDepthSampler();
    //pContext->PSSetSamplers(0, 1, &depthSampler);
    //m_finalRenderMesh->Render(pContext);
    //return;

    // オフスクリーンMRTに再バインド
    targets[0] = m_offscreenRTwithMSAA;
	targets[1] = m_brightRTwithMSAA;
    dsv = m_offscreenRTwithMSAA->GetDSV();
    RenderTarget::BindMultiple(pContext, 2, targets, dsv);

    // ===== Lighting Pass =====
    m_deferredLightingPass->Execute(pContext, m_gBufferPass, ssaoSRV, m_dsStates, m_finalRenderMesh);


    // ─── 工程1: 不透明パス ───
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); // 通常の深度テスト
    m_renderQueues[opaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates,true);


    // ─── InstancedModelの描画 ───
    //未設定時は早期returnされるため無害
    m_pInstancedModel->Render(pContext);

    // ─── スカイボックスの描画 ───
    if (m_pSkyBox) {
        m_rasterStates->Bind(pContext, RasterizerStates::CullMode::None);       // 前面カリングで内側を描画
        m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthLessEqual);    // DepthLessEqual で遠平面にフィット

        pContext->VSSetShader(nullptr,nullptr,0);
        pContext->PSSetShader(nullptr, nullptr, 0);
        pContext->GSSetShader(nullptr, nullptr, 0);
        // 描画実行
        m_pSkyBox->Draw(pContext, m_currentCamera->GetViewMatrix(), m_currentCamera->GetProjectionMatrix());
    }

    
    // ─── 工程3: 半透明パス ───
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); 
    m_renderQueues[transparentIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates,true);


    
    // ==========================================
    // 3. ポストプロセス・ピンポン・パイプライン
    // ==========================================
    // ピンポン用の入力・出力RTポインタ
    RenderTarget* pCurrentInput = m_offscreenRT; 
    RenderTarget* pCurrentOutput = m_tmpRT;      

    //これ以前でMSAAレンダリングした内容をm_offscreenRTにダウンサンプリング描画
    ID3D11Texture2D* offScreenRTTex = m_offscreenRT->GetTexture(); // 描画先
    ID3D11Texture2D* msaaTex = m_offscreenRTwithMSAA->GetTexture(); //描画元
    //Resolve（解像）を実行して画面に直接転写する
    pContext->ResolveSubresource(
        offScreenRTTex, 0,           // 転送先: 非MSAA RT
        msaaTex, 0,                 // 転送元: MSAA RT
        DXGI_FORMAT_R16G16B16A16_FLOAT 
    );
	offScreenRTTex = m_brightRT->GetTexture(); // 描画先
	msaaTex = m_brightRTwithMSAA->GetTexture(); //描画元
    pContext->ResolveSubresource(
        offScreenRTTex, 0,
        msaaTex, 0,
        DXGI_FORMAT_R16G16B16A16_FLOAT
    );

    // ========================================================
    // 輝度抽出RTに対して独立してBlurを実行
    // ========================================================

    // Bloom用Blurの入力・出力RT（シーンRTとは分離）
    RenderTarget * pBlurInput = m_brightRT;     // 最初の入力：輝度抽出テクスチャ
    RenderTarget* pBlurOutput = m_tmpRT;        // 作業バッファ1

    // Bloom合成用にブラー結果を設定
    ID3D11ShaderResourceView* bloomSRV = 
        m_bloomBlurPass->Execute( pContext, m_brightRT,m_finalRenderMesh);
    m_finalRenderBloomCombinePostProcess->SetBrightBlurTexture(bloomSRV);


    pCurrentInput = m_offscreenRT; // 通常の3Dシーンの絵（Resolve直後の状態）に戻す
    pCurrentOutput = m_tmpRT;      // 出力先をリセット


    // ========================================================
    // 4. ポストプロセス・ピンポン・チェーン
    // ========================================================
    // ポストプロセスチェーン実行（戻り値が最終結果RT）
        RenderTarget * finalResult = m_postProcessChain->Render(
            pContext, m_offscreenRT, m_tmpRT, m_finalRenderMesh);
    
    // ==========================================
    // 4. 出力先を「デフォルト（画面）」に戻して最終転写
    // ==========================================
    m_graphics->bindDefaultRenderTarget(); // バックバッファに切り替え
    m_blendStates->Bind(pContext, BlendMode::Opaque);
	UpdatePostProcessConstantBuffer();//ポストプロセス用の定数バッファを更新

    m_finalRenderScreenBlitPostProcess->Render(pContext, finalResult);//pCurrentInput
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


bool Renderer::createFinalRenderQuad() {
    
    ID3D11Device* pDevice = m_graphics->GetDevice();
    if (!pDevice)return false;
    m_finalRenderMesh = Mesh::CreateQuad(pDevice);
    if (!m_finalRenderMesh)return false;

    return true;
}


void Renderer::UpdatePostProcessConstantBuffer()
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    PostProcessConstantBuffer postParams;
    postParams.exposure = m_postProcessData.exposure;
    postParams.gammaCorrection = m_postProcessData.gammaCorrection;
    postParams.padding[0] = 0.0f;
    postParams.padding[1] = 0.0f;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = pContext->Map(m_pPostProcessCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(mapped.pData, &postParams, sizeof(PostProcessConstantBuffer));
        pContext->Unmap(m_pPostProcessCB.Get(), 0);
    }

    ID3D11Buffer* cbArray[] = { m_pPostProcessCB.Get() };
    pContext->VSSetConstantBuffers(5, 1, cbArray);
    pContext->PSSetConstantBuffers(5, 1, cbArray);
    pContext->GSSetConstantBuffers(5, 1, cbArray);
}

void Renderer::SetDirectionalLightDirection(DirectX::XMFLOAT3 dir)
{
    if (m_shadowSystem) {
        m_shadowSystem->SetDirectionalLightDirection(dir);
    }
}

void Renderer::SetPointLightPosition(DirectX::XMFLOAT3 pos)
{
    if (m_shadowSystem) {
        m_shadowSystem->SetPointLightPosition(pos);
    }
}

void Renderer::SetLightVisibilityMode(LightVisibilityMode mode)
{
    if (m_shadowSystem) {
        m_shadowSystem->SetLightVisibilityMode(mode);
    }
}

void Renderer::SetInstanceCount(UINT count)
{
    if (m_pInstancedModel) {
        m_pInstancedModel->SetActiveCount(count);
    }
}

void Renderer::SetBloomActive(bool isOn)
{
    if (m_finalRenderBloomCombinePostProcess) {
        m_finalRenderBloomCombinePostProcess->SetActive(isOn);
    }
}

bool Renderer::IsBloomActive() const
{
    return m_finalRenderBloomCombinePostProcess ? m_finalRenderBloomCombinePostProcess->IsActive() : false;
}

void Renderer::SetInstanceCount(UINT count, DirectX::XMFLOAT3 offset)
{
    if (m_pInstancedModel) {
        m_pInstancedModel->SetActiveCount(count, offset);
    }
}