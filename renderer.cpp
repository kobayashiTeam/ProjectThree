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
//test
#include"postProcessChain.h"
#include"bloomBlurPass.h"
#include"gBufferPass.h"
#include"ssaoPass.h"
#include"shadowSystem.h"

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

	// 3. オフスクリーンレンダーターゲット1号の初期化（テスト）
		//HDRの実験のため、colorFormatを16bit浮動小数点にしてみる
	m_offscreenRT = new RenderTarget();
	if (!m_offscreenRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
		return false;
	}

    m_offscreenRTwithMSAA = new RenderTarget();
    if (!m_offscreenRTwithMSAA->InitializeWithMSAA(
        pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }

        //オフスクリーンレンダー２号の初期化（２号というか２枚で十分、swapChainで使う）
    m_tmpRT = new RenderTarget();
    if (!m_tmpRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
        //test bloom用
	m_brightRT = new RenderTarget();
	if (!m_brightRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
		return false;
	}
	m_brightRTwithMSAA = new RenderTarget();
    if (!m_brightRTwithMSAA->InitializeWithMSAA(
        pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
	

    //テスト:ポストプロセス
    //simpleBlit
    m_finalRenderScreenBlitPostProcess = new ScreenBlitPostProcess();
    m_finalRenderScreenBlitPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::ScreenBlit));
 

   //正式にクラス化したPostProcessChainの生成、初期化
        //ScreenBlitPostProcess（最終転写用）とHorizontalBlur/VerticalBlur（Bloomの中間ブラー用）は
	    //特殊なのでchainに加えない。個別に使う
        //各postprocessはrendererメンバである必要もなくなり、add時に生成、代入
	m_postProcessChain = new PostProcessChain();
    m_postProcessChain->AddEffect<MonochromePostProcess>(pDevice, ShaderID::Monochromatic, false);
    m_postProcessChain->AddEffect<InversionPostProcess>(pDevice, ShaderID::Inversion, false);
    m_postProcessChain->AddEffect<SepiaPostProcess>(pDevice, ShaderID::Sepia, false);
    m_postProcessChain->AddEffect<SimpleBoxBlurPostProcess>(pDevice, ShaderID::SimpleBoxBlur, false);
    m_postProcessChain->AddEffect<SharpenPostProcess>(pDevice, ShaderID::Sharpen, false);
    m_postProcessChain->AddEffect<VignettePostProcess>(pDevice, ShaderID::Vignette, false);
    m_finalRenderBloomCombinePostProcess= m_postProcessChain->AddEffect<BloomCombinePostProcess>(pDevice, ShaderID::BloomCombine, true);

        //bloomBlurだけpostprocesschainパスとは別パスとして別クラスに生成
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

    //instancedModel:ここでmeshをつくる
    m_pInstancedModel = new InstancedModel();
    if (!m_pInstancedModel->Init(pDevice, pContext,Mesh::CreateCube(pDevice, 1.0f), 27))return false;
    

    //新規:ShadowSystem生成初期化
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

    //遅延シェーディング用のバッファの初期化
        //シェーダ初期化
	m_pDeferredLightingShader = ShaderManager::GetInstance().GetShader(ShaderID::DeferredLighting);

        //専用サンプラー初期化
    D3D11_SAMPLER_DESC pointDesc = {};
    pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // 深度は補間NG、点サンプル必須
    pointDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    pointDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    pointDesc.MinLOD = 0;
    pointDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState(&pointDesc, m_gBufferDepthSampler.GetAddressOf());
    if (FAILED(hr)) return false;

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
    //test:PSにもこれを設定
    pContext->PSSetConstantBuffers(0, 1, cbArray);
    //テスト：GSにも同cbを設定
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

    int opaqueIdx = static_cast<int>(RenderPass::Opaque);
    int outlineIdx = static_cast<int>(RenderPass::Outline);
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
    // 【新設】1. 描画先を「自作の裏画面」に切り替える（Offscreen Pass 開始）
    // ==========================================
    m_offscreenRTwithMSAA->Clear(pContext);//テスト：
	m_brightRTwithMSAA->Clear(pContext);//テスト：

    // 配列にして準備
    RenderTarget* targets[2] = {
        m_offscreenRTwithMSAA,
        m_brightRTwithMSAA
    };
    // 深度バッファは代表して1つ目のものから取得して渡す
    ID3D11DepthStencilView* dsv = m_offscreenRTwithMSAA->GetDSV();
    //staticメソッドで綺麗にバインド！
    RenderTarget::BindMultiple(pContext, 2, targets, dsv);


    //-----Lighting Pass-----
    m_shadowSystem->BindForLighting(pContext);  // シャドウマップ関連の設定

    //-----新規工程:deferred不透明パス-----
	m_gBufferPass->Begin(pContext);             // G-Bufferのレンダーターゲットに切り替え

    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest);

    m_renderQueues[deferredOpaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates, true);


    // ==========================================
    // 【★ここに新設！】SSAO 生成 ＆ SSAOブラー パス
    // ==========================================
    
    ID3D11ShaderResourceView* ssaoSRV =m_ssaoPass->Execute(
        pContext, 
        m_gBufferPass->GetNormalSRV(), 
        m_gBufferPass->GetPositionSRV(), 
        m_finalRenderMesh);

    // ===== ここで既存の「裏画面」MRTバインドに戻す =====
    targets[0] = m_offscreenRTwithMSAA;
	targets[1] = m_brightRTwithMSAA;
    dsv = m_offscreenRTwithMSAA->GetDSV();
    RenderTarget::BindMultiple(pContext, 2, targets, dsv);

    // ===== 【新設】Lighting Pass =====
    // G-Buffer 3枚をSRVとしてバインド(t8,t9,t10,t11,t12など、シャドウマップとぶつからない番号で)
    ID3D11ShaderResourceView* gbufferSRVs[5] = {
     m_gBufferPass->GetAlbedoSRV(),   // t8//m_gBufferAlbedo->GetSRV()
     m_gBufferPass->GetNormalSRV(),   // t9//m_gBufferNormal->GetSRV()
     m_gBufferPass->GetPositionSRV(),  // t10//m_gBufferPosition->GetSRV()
     m_gBufferPass->GetDepthSRV(),   //t11//m_gBufferDepthOnly->GetSRV()
     ssaoSRV       // ★t12 にSSAOを滑り込ませる！
    };
    pContext->PSSetShaderResources(8, 5, gbufferSRVs);

    m_pDeferredLightingShader->Bind(pContext); // VS+PS(フルスクリーンクアッド用)
    ID3D11SamplerState* pointSampler = m_gBufferDepthSampler.Get();
    pContext->PSSetSamplers(3, 1, &pointSampler); // ★追加
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); // SV_Depthで書き込むため必要
    m_finalRenderMesh->Render(pContext);


    // ─── 工程1: 不透明パス ───
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); // 通常の深度テスト
    m_renderQueues[opaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates,true);

    // ─── 【新設】PointSpriteの描画 ───
    //m_pPointSpriteGSEffect->Draw(pContext);

    // ─── 【新設】InstancedModelの描画 ───
    //m_pInstancedModel->Render(pContext);

    // ─── 【ここ！！】スカイボックスの描画 ───
    // ─── 【新設】スカイボックスの描画 ───
    if (m_pSkyBox) {
        // 境目でステートをスカイボックス用に切り替える！front,depthlessequal
        m_rasterStates->Bind(pContext, RasterizerStates::CullMode::None);       // 内側を見せるため前面カリング
        m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthLessEqual);    // 1.0の隙間に滑り込ませる

        //test:ごちゃごちゃしたシェーダセットを一掃
        pContext->VSSetShader(nullptr,nullptr,0);
        pContext->PSSetShader(nullptr, nullptr, 0);
        pContext->GSSetShader(nullptr, nullptr, 0);
        // 描画実行
        m_pSkyBox->Draw(pContext, m_currentCamera->GetViewMatrix(), m_currentCamera->GetProjectionMatrix());
    }

    
    //return;
    
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
    m_renderQueues[transparentIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates,true);

    //return;

    
    // ==========================================
    // 【新設】3. ポストプロセス・ピンポン・パイプライン
    // ==========================================
    // 現在の「入力（読む）」と「出力（書く）」の追跡用ポインタ
    RenderTarget* pCurrentInput = m_offscreenRT; // 3Dシーンが描き込まれている
    RenderTarget* pCurrentOutput = m_tmpRT;      // まだ空っぽの作業机

    //これ以前でMSAAレンダリングした内容をm_offscreenRTにダウンサンプリング描画
    ID3D11Texture2D* offScreenRTTex = m_offscreenRT->GetTexture(); // 描画先
    ID3D11Texture2D* msaaTex = m_offscreenRTwithMSAA->GetTexture(); //描画元
    //Resolve（解像）を実行して画面に直接転写する
    pContext->ResolveSubresource(
        offScreenRTTex, 0,           // 転送先：本物の画面
        msaaTex, 0,                 // 転送元：自作MSAAバッファ
        DXGI_FORMAT_R16G16B16A16_FLOAT // フォーマット（お使いのものに合わせる）
    );
    // 3DシーンのResolveの直後あたりに追記
	offScreenRTTex = m_brightRT->GetTexture(); // 描画先
	msaaTex = m_brightRTwithMSAA->GetTexture(); //描画元
    pContext->ResolveSubresource(
        offScreenRTTex, 0,
        msaaTex, 0,
        DXGI_FORMAT_R16G16B16A16_FLOAT
    );

    // ========================================================
    // 2. 【ここ！】Resolveされて中身が入った m_brightRT を使って、独立してBlurを2回叩く
    // ========================================================

    // ボケ専用のピンポン用ポインタ（m_offscreenRTには絶対に触れさせない）
    RenderTarget * pBlurInput = m_brightRT;     // 最初の入力：輝度抽出テクスチャ
    RenderTarget* pBlurOutput = m_tmpRT;        // 作業バッファ1

    //// 3. 【重要】完成したボケ画像を、Bloom合成エフェクトに仕込む！
    // Bloom下ごしらえ（前回の話）
    ID3D11ShaderResourceView* bloomSRV = 
        m_bloomBlurPass->Execute( pContext, m_brightRT,m_finalRenderMesh);
    m_finalRenderBloomCombinePostProcess->SetBrightBlurTexture(bloomSRV);


    // 次のチェーン（モノクロやビネットなど）に行くための「お片付け」
    // ⚠️今のままだと、pCurrentInput が「ボケ画像」になってしまっていて、
    // 通常の3Dシーン（m_offscreenRT）が迷子になっています。
    // なので、メインチェーンを始めるために、ポインタを本来の3Dシーンの場所に戻してあげます。

    pCurrentInput = m_offscreenRT; // 通常の3Dシーンの絵（Resolve直後の状態）に戻す
    pCurrentOutput = m_tmpRT;      // 出力先も綺麗にリセット


    // ========================================================
    // 4. ポストプロセス・ピンポン・チェーン
    // ========================================================
    //// チェーン実行。戻り値が「最終的にどのRTに絵が入っているか」を教えてくれる
        RenderTarget * finalResult = m_postProcessChain->Render(
            pContext, m_offscreenRT, m_tmpRT, m_finalRenderMesh);
    
    //return;

    // ==========================================
    // 4. 出力先を「デフォルト（画面）」に戻して最終転写
    // ==========================================
    m_graphics->bindDefaultRenderTarget(); // 本物の画面をセット＋クリア
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

void Renderer::BeginStencilOutlinePass()
{
    // 将来的にステンシルマスクを有効化するステート変更をここに記述
}

void Renderer::EndStencilOutlinePass()
{
    // ステンシルマスクを元に戻す処理をここに記述
}

bool Renderer::createFinalRenderQuad() {
    
    ID3D11Device* pDevice = m_graphics->GetDevice();
    if (!pDevice)return false;
    m_finalRenderMesh = Mesh::CreateQuad(pDevice);//mesh
    if (!m_finalRenderMesh)return false;

    return true;
}


void Renderer::UpdatePostProcessConstantBuffer()
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    PostProcessConstantBuffer postParams;
    postParams.exposure = m_postProcessData.exposure;
    postParams.padding[0] = 0.0f;
    postParams.padding[1] = 0.0f;
    postParams.padding[2] = 0.0f;

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
