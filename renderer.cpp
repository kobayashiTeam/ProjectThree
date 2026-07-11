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
	m_blurPingRT = new RenderTarget();
	if (!m_blurPingRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
		return false;
	}

    //テスト:ポストプロセス
    //simpleBlit
    m_finalRenderScreenBlitPostProcess = new ScreenBlitPostProcess();
    m_finalRenderScreenBlitPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::ScreenBlit));
    //monochrome
    m_finalRenderMonochromePostProcess = new MonochromePostProcess();
    m_finalRenderMonochromePostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::Monochromatic));
        //テスト：効果オフ
    m_finalRenderMonochromePostProcess->SetActive(false);
    //Inversion
    m_finalRenderInversionPostProcess = new InversionPostProcess();
    m_finalRenderInversionPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::Inversion));
        //テスト：効果オフ
    m_finalRenderInversionPostProcess->SetActive(false);
    //sepia
    m_finalRenderSepiaPostProcess = new SepiaPostProcess();
    m_finalRenderSepiaPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::Sepia));
        //テスト：効果オフ
    m_finalRenderSepiaPostProcess->SetActive(false);
    //simpleBoxBlur
    m_finalRenderSimpleBoxBluer = new SimpleBoxBlurPostProcess();
    m_finalRenderSimpleBoxBluer->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::SimpleBoxBlur));
        //テスト：効果オフ
    m_finalRenderSimpleBoxBluer->SetActive(false);
    //sharpen
    m_finalRenderSharpenPostProcess = new SharpenPostProcess();
    m_finalRenderSharpenPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::Sharpen));
        //テスト：効果オフ
    m_finalRenderSharpenPostProcess->SetActive(false);
    //vignette
    m_finalRenderVignettePostProcess = new VignettePostProcess();
    m_finalRenderVignettePostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::Vignette));
        //テスト：効果オフ
    m_finalRenderVignettePostProcess->SetActive(false);
    //HoriBlur
	m_finalRenderHorizontalBlurPostProcess = new HorizontalBlurPostProcess();
	m_finalRenderHorizontalBlurPostProcess->Initialize(pDevice,
		ShaderManager::GetInstance().GetShader(ShaderID::HoriBlur));
	//VerBlur
	m_finalRenderVerticalBlurPostProcess = new VerticalBlurPostProcess();
	m_finalRenderVerticalBlurPostProcess->Initialize(pDevice,
		ShaderManager::GetInstance().GetShader(ShaderID::VerBlur));
	//BloomCombine
	m_finalRenderBloomCombinePostProcess = new BloomCombinePostProcess();
	m_finalRenderBloomCombinePostProcess->Initialize(pDevice,
		ShaderManager::GetInstance().GetShader(ShaderID::BloomCombine));

    //自動実行チェーン（配列）に、適用したい「順番通り」に登録する
        // ※ 最終転写用のBlitは「画面に出力する特殊枠」にするため、ここには入れません
    m_postProcessChain.push_back(m_finalRenderMonochromePostProcess);
    m_postProcessChain.push_back(m_finalRenderInversionPostProcess);
    m_postProcessChain.push_back(m_finalRenderSepiaPostProcess);
    m_postProcessChain.push_back(m_finalRenderSimpleBoxBluer);
    m_postProcessChain.push_back(m_finalRenderSharpenPostProcess);
    m_postProcessChain.push_back(m_finalRenderVignettePostProcess);
        //test:blur参加
	m_postProcessChain.push_back(m_finalRenderBloomCombinePostProcess);

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
    

    //ライト①（Directional）
        //cb生成
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(LightBufferCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pDevice->CreateBuffer(&bd, nullptr, &m_lightCB);
        //ライト生成
    // Renderer初期化時など
    DirectionalLight dirLight = {};
    dirLight.type = LightType::Directional;
    dirLight.position = { -3.0f, 5.0f, -10.0f };//-3,5,-10
	dirLight.direction = { 3.0f, -1.0f, 1.0f };//
    dirLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    dirLight.intensity = 0.40f;//0.15f
        //m_lights要素数登録
    m_directionalLights.reserve(MAX_LIGHTS);
    m_directionalLights.push_back(dirLight);//test:空にしてみる

    //シャドウマップ(Directional Light)
    ShadowMap shadowMap;
    shadowMap.Initialize(pDevice,2048);
    shadowMap.setLight(&m_directionalLights[0]);
        //ライトとの組み合わせ、事前に配列予約
    m_shadowMaps.reserve(MAX_LIGHTS);
    m_shadowMaps.push_back(shadowMap);
        //shadowシェーダ設定
    m_pShadowShader = ShaderManager::GetInstance().GetShader(ShaderID::Shadow);
        //サンプラー生成
    // Renderer初期化時に作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.BorderColor[0] = 1.0f; // 範囲外は影なし
    sampDesc.BorderColor[1] = 1.0f;
    sampDesc.BorderColor[2] = 1.0f;
    sampDesc.BorderColor[3] = 1.0f;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

    pDevice->CreateSamplerState(&sampDesc, &m_shadowSampler);


    //ライト②（Point）
        //cb生成
    bd = {};
    bd.ByteWidth = sizeof(ShadowCubeCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pDevice->CreateBuffer(&bd, nullptr, &m_pointLightCB);
        //ライト生成
    PointLight  pointLight = {};
    pointLight.position = { 5.0f, 5.0f, 3.0f };
    pointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLight.intensity = 0.4f;//0.6f
        //m_lights要素数登録
    m_pointLights.reserve(MAX_LIGHTS);
    m_pointLights.push_back(pointLight);
        //シャドウキューブマップ（Point Light）
    ShadowCubeMap shadowCubeMap;
    shadowCubeMap.Initialize(pDevice,2048);
    shadowCubeMap.SetLight(&pointLight);
        //ライトとの組み合わせ、事前に配列予約
    m_shadowCubeMaps.reserve(MAX_LIGHTS);
    m_shadowCubeMaps.push_back(shadowCubeMap);
        //shadowシェーダ設定
    m_pShadowCubeShader = ShaderManager::GetInstance().GetShader(ShaderID::ShadowCube);
        //GS設定
    m_pShadowCubeGS = ShaderManager::GetInstance().getGS(ShaderID::ShadowCubeGS);
        //通常サンプラー生成
    sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    pDevice->CreateSamplerState(&sampDesc, &m_shadowCubeSampler);


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
    m_gBufferPosition = new RenderTarget();
    if (!m_gBufferPosition->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
	m_gBufferNormal = new RenderTarget();
	if (!m_gBufferNormal->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
		return false;
	}
	m_gBufferAlbedo = new RenderTarget();
    if (!m_gBufferAlbedo->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
	m_gBufferDepthOnly = new RenderTarget();
	if (!m_gBufferDepthOnly->InitializeDepthOnly(pDevice, 1280, 720)) {
		return false;
	}
        //シェーダ初期化
	m_pDeferredGBufferShader = ShaderManager::GetInstance().GetShader(ShaderID::DeferredGB);
	m_pDeferredLightingShader = ShaderManager::GetInstance().GetShader(ShaderID::DeferredLighting);

        //専用サンプラー初期化
    // Renderer::Initialize() などの中
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

    
    //SSAO
        //rtの初期化（後で確認）
    m_ssaoRawRT = new RenderTarget();
    if (!m_ssaoRawRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
    m_ssaoBlurRT = new RenderTarget();
    if (!m_ssaoBlurRT->Initialize(pDevice, 1280, 720, DXGI_FORMAT_R16G16B16A16_FLOAT)) {
        return false;
    }
        //ノイズテクスチャの生成
    if (!initSSAO(pDevice))return false;

        //サンプラーの生成
	initSSAOSampler(pDevice);
	    //半球サンプルの定数バッファ生成

    return true;

}

void Renderer::BeginFrame(Camera* camera, float r, float g, float b, float a)
{
    m_currentCamera = camera;
    m_graphics->BeginScene(r, g, b, a);

    // 共通のトポロジー設定
    m_graphics->GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // フレームごとの定数バッファ更新(b0)
    UpdatePerFrameConstantBuffer();
        //DirectionalLightも共通なので送る(b3)
    UpdateLightDataConstantBuffer();
        //PointLightも送る(b4)
    UpdatePointLightConstantBuffer();
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

    // ===== 1パス目：シャドウマップ生成 =====
    // ===== DirectionalLight のシャドウパス =====
    m_shadowMaps[0].BeginRender(pContext);
    // シャドウ用VSをバインド
    m_pShadowShader->Bind(pContext);
    //pContext->PSSetShader(nullptr, nullptr, 0);//二度手間だが一度セットした空PSを外す
    // ライトのView/Projをcbufferに送る（LightBufferはすでにb3にある）
    // 不透明オブジェクトのみ描画（PerObjectCBだけ更新すればOK）
    //SubmitShadowPass();//対象renderQueueのコマンド内容を変える
    m_renderQueues[static_cast<int>(RenderPass::Opaque)].
        ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates,false);
    m_renderQueues[static_cast<int>(RenderPass::DeferredOpaque)].
        ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_shadowMaps[0].EndRender(pContext);
    // オーバーライドをリセット
    m_renderQueues[static_cast<int>(RenderPass::Opaque)].SetOverrideVS(nullptr);


    // ===== PointLight のシャドウパス =====
    m_shadowCubeMaps[0].BeginRender(pContext);
    m_pShadowCubeShader->Bind(pContext);  // VS+PS
    pContext->GSSetShader(m_pShadowCubeGS,nullptr,0);// GS(直接代入)
    //pContext->PSSetShader(nullptr, nullptr, 0);
    m_renderQueues[static_cast<int>(RenderPass::Opaque)].
        ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_renderQueues[static_cast<int>(RenderPass::DeferredOpaque)].
        ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_shadowCubeMaps[0].EndRender(pContext);

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
    //m_offscreenRTwithMSAA->Bind(pContext); // ※前回統合した自作のレンダーターゲット
    //staticメソッドで綺麗にバインド！
    RenderTarget::BindMultiple(pContext, 2, targets, dsv);
    // Directionalライトの為のシャドウマップをt3に、比較用サンプラーもバインド
    auto* srv = m_shadowMaps[0].GetSRV();
    pContext->PSSetShaderResources(3, 1, &srv);
    ID3D11SamplerState* sampler = m_shadowSampler.Get();
    pContext->PSSetSamplers(1, 1, &sampler); 
    //Pointライトの為のシャドウマップをt4に、キューブ用サンプラーもバインド
    srv = m_shadowCubeMaps[0].GetSRV();
    pContext->PSSetShaderResources(4,1,&srv);
    sampler = m_shadowCubeSampler.Get();
    pContext->PSSetSamplers(2,1,&sampler);

	//2. 各パスのキューを、適切なステートをセットしてから実行する

    int opaqueIdx = static_cast<int>(RenderPass::Opaque);
    int outlineIdx = static_cast<int>(RenderPass::Outline);
    int transparentIdx = static_cast<int>(RenderPass::Transparent);
    int deferredOpaqueIdx = static_cast<int>(RenderPass::DeferredOpaque);

    //-----新規工程:deferred不透明パス-----
    m_gBufferPosition->Clear(pContext);
    m_gBufferNormal->Clear(pContext);
    m_gBufferAlbedo->Clear(pContext);
	m_gBufferDepthOnly->Clear(pContext);

    RenderTarget* gbufferTargets[3] = {
    m_gBufferAlbedo, m_gBufferNormal, m_gBufferPosition
    };
    ID3D11DepthStencilView* gbufferDSV = m_gBufferDepthOnly ->GetDSV(); // 非MSAA専用の深度
    RenderTarget::BindMultiple(pContext, 3, gbufferTargets, gbufferDSV);

    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest);

    m_renderQueues[deferredOpaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates, true);


    // ==========================================
    // 【★ここに新設！】SSAO 生成 ＆ SSAOブラー パス
    // ==========================================
    // 1. SSAO用のテンポラリRT（非MSAA）をクリアしてバインド
    m_ssaoRawRT->Clear(pContext);
    m_ssaoRawRT->Bind(pContext); // 深度は不要（nullptr）でも都合上できてしまう？
    
    // 2. G-Buffer（Albedo以外）をSRVとしてセット。ノイズテクスチャ、半球サンプル、ビュー行列をセット
    ID3D11ShaderResourceView* ssaoInputs[3] = {
        m_gBufferNormal->GetSRV(),
        m_gBufferPosition->GetSRV(),
        m_ssaoNoiseTextureSRV.Get() // ランダム回転用ノイズ
    };
    pContext->PSSetShaderResources(0, 3, ssaoInputs);
    // 3. フルスクリーンクアッド（m_finalRenderMesh）でSSAOシェーダーを実行 → m_ssaoRawRT へ
    m_pSSAOShader->Bind(pContext);
    m_finalRenderMesh->Render(pContext);
    // 4. m_ssaoRawRT を入力にして、エッジ保存ブラーを実行 → m_ssaoBlurRT（完成品）へ
    m_ssaoBlurRT->Clear(pContext);
    m_ssaoBlurRT->Bind(pContext);

    auto* rawSsaoSRV = m_ssaoRawRT->GetSRV();
    pContext->PSSetShaderResources(0, 1, &rawSsaoSRV);

    m_pSSAOBlurShader->Bind(pContext);
    m_finalRenderMesh->Render(pContext);

    // ===== ここで既存の「裏画面」MRTバインドに戻す =====
    targets[0] = m_offscreenRTwithMSAA;
	targets[1] = m_brightRTwithMSAA;
    dsv = m_offscreenRTwithMSAA->GetDSV();
    RenderTarget::BindMultiple(pContext, 2, targets, dsv);

    // ===== 【新設】Lighting Pass =====
    // G-Buffer 3枚をSRVとしてバインド(t8,t9,t10,t11,t12など、シャドウマップとぶつからない番号で)
    ID3D11ShaderResourceView* gbufferSRVs[5] = {
     m_gBufferAlbedo->GetSRV(),   // t8
     m_gBufferNormal->GetSRV(),   // t9
     m_gBufferPosition->GetSRV(),  // t10
     m_gBufferDepthOnly->GetSRV(),   //t11
     m_ssaoBlurRT->GetSRV()       // ★t12 にSSAOを滑り込ませる！
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

    // ─── パスA: 横ボケ ───
    pBlurOutput->Clear(pContext);
    pBlurOutput->Bind(pContext);
    m_finalRenderHorizontalBlurPostProcess->Render(pContext, pBlurInput);
    m_finalRenderMesh->Render(pContext);

    // ─── パスB: 縦ボケ ───
    m_blurPingRT->Clear(pContext);   // ★新規で用意するバッファ
    m_blurPingRT->Bind(pContext);
    m_finalRenderVerticalBlurPostProcess->Render(pContext, pBlurOutput);
    m_finalRenderMesh->Render(pContext);

    // 3. 【重要】完成したボケ画像を、Bloom合成エフェクトに仕込む！
    // 完成したボケ画像は m_blurPingRT に入っている
    m_finalRenderBloomCombinePostProcess->SetBrightBlurTexture(m_blurPingRT->GetSRV());



    // 次のチェーン（モノクロやビネットなど）に行くための「お片付け」
    // ⚠️今のままだと、pCurrentInput が「ボケ画像」になってしまっていて、
    // 通常の3Dシーン（m_offscreenRT）が迷子になっています。
    // なので、メインチェーンを始めるために、ポインタを本来の3Dシーンの場所に戻してあげます。

    pCurrentInput = m_offscreenRT; // 通常の3Dシーンの絵（Resolve直後の状態）に戻す
    pCurrentOutput = m_tmpRT;      // 出力先も綺麗にリセット


    // ========================================================
    // 4. ポストプロセス・ピンポン・チェーン
    // ========================================================
    for (PostProcess* effect : m_postProcessChain)
    {
        // 無効化されているエフェクト（例: 今は狂気度が低いからモノクロOFFなど）はスキップ
        if (!effect->IsActive()) continue;

        // 出力先をバインドしてクリア
        pCurrentOutput->Clear(pContext);
        pCurrentOutput->Bind(pContext);

        // 描画（入力テクスチャを渡して、Quadを描画）
        effect->Render(pContext, pCurrentInput);
        m_finalRenderMesh->Render(pContext);

        // 自動でピンポン（入力と出力を入れ替える）
        std::swap(pCurrentInput, pCurrentOutput);
    }
    
    //return;

    // ==========================================
    // 4. 出力先を「デフォルト（画面）」に戻して最終転写
    // ==========================================
    m_graphics->bindDefaultRenderTarget(); // 本物の画面をセット＋クリア
    m_blendStates->Bind(pContext, BlendMode::Opaque);
	UpdatePostProcessConstantBuffer();//ポストプロセス用の定数バッファを更新

    m_finalRenderScreenBlitPostProcess->Render(pContext, pCurrentInput);
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


void Renderer::UpdateLightDataConstantBuffer()
{
    LightBufferCB cb = {};

    // Directionalを先に詰める
    for (int i = 0; i < (int)m_directionalLights.size() && cb.lightCount < MAX_LIGHTS; i++)
    {
        const DirectionalLight& L = m_directionalLights[i];
        auto& dst = cb.lights[cb.lightCount];
        dst.position = { L.position.x, L.position.y, L.position.z, 0.0f };
        dst.direction = { L.direction.x, L.direction.y, L.direction.z, 0.0f };
        dst.color = L.color;
        dst.intensity = L.intensity;
        dst.type = (int)LightType::Directional;
        dst.farPlane = 0.0f;  // 未使用
        DirectX::XMMATRIX lsm = L.GetViewMatrix() * L.GetProjectionMatrix();
        dst.lightSpaceMatrix = DirectX::XMMatrixTranspose(lsm);
        cb.lightCount++;
    }

    // Pointを続けて詰める
    for (int i = 0; i < (int)m_pointLights.size() && cb.lightCount < MAX_LIGHTS; i++)
    {
        const PointLight& L = m_pointLights[i];
        auto& dst = cb.lights[cb.lightCount];
        dst.position = { L.position.x, L.position.y, L.position.z, 0.0f };
        dst.color = L.color;
        dst.intensity = L.intensity;
        dst.type = (int)LightType::Point;
        dst.farPlane = L.farPlane;
        // lightSpaceMatrixは未使用なのでゼロのまま
        cb.lightCount++;
    }

    auto* ctx = m_graphics->GetContext();
    ctx->UpdateSubresource(m_lightCB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbArray[] = { m_lightCB.Get() };
    ctx->VSSetConstantBuffers(3, 1, cbArray);
    ctx->PSSetConstantBuffers(3, 1, cbArray);
}


void Renderer::UpdatePointLightConstantBuffer()
{
    if (m_pointLights.empty()) return;

    ShadowCubeCB cb = {};
    const PointLight& L = m_pointLights[0];  // 今は1灯固定

    cb.gLightPos = L.position;
    cb.gFarPlane = L.farPlane;

    // 6面分のViewProj行列
    DirectX::XMMATRIX proj = L.GetProjectionMatrix();
    for (int i = 0; i < 6; i++)
    {
        DirectX::XMMATRIX vp = L.GetViewMatrix(i) * proj;
        cb.gLightViewProj[i] = DirectX::XMMatrixTranspose(vp);
    }

    auto* ctx = m_graphics->GetContext();
    ctx->UpdateSubresource(m_pointLightCB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbArray[] = { m_pointLightCB.Get() };
    ctx->VSSetConstantBuffers(4, 1, cbArray);
    ctx->GSSetConstantBuffers(4, 1, cbArray);  // GSにも忘れず
    ctx->PSSetConstantBuffers(4, 1, cbArray);
}


// シャドウパス用にキューに積むとき
void Renderer::SubmitShadowPass()
{
    m_renderQueues[static_cast<int>(RenderPass::Opaque)].SetOverrideVS(m_pShadowShader);
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


bool Renderer::initSSAO(ID3D11Device* pDevice) {

    ID3D11Texture2D* m_pNoiseTexture = nullptr;
    // 1. もとになるノイズ色（ベクトル）配列をプログラム内で生成
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    // 4x4 = 16画素分のデータ。高精度なFLOATフォーマットを使うため DirectX::XMFLOAT4 を使用
    DirectX::XMFLOAT4 noiseValues[16];
    for (int i = 0; i < 16; ++i)
    {
        noiseValues[i].x = dis(gen); // -1.0 ～ 1.0 のランダム
        noiseValues[i].y = dis(gen); // -1.0 ～ 1.0 のランダム
        noiseValues[i].z = 0.0f;     // 接空間のZ（法線方向）は回転させないので 0
        noiseValues[i].w = 0.0f;     // 未使用
    }

    // 2. テクスチャの設定（ID3D11Texture2D）を定義
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 4;                       // 4マス
    texDesc.Height = 4;                       // 4マス
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 浮動小数点の高精度フォーマット
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;     // GPUから読み込むだけなのでDEFAULT
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // シェーダーに渡す用
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    // 作成と同時に、さっき作った配列データを初期データとして流し込む
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = noiseValues;
    initData.SysMemPitch = 4 * sizeof(DirectX::XMFLOAT4); // 横1行分のバイト数
    initData.SysMemSlicePitch = 0;

    HRESULT hr = pDevice->CreateTexture2D(&texDesc, &initData, &m_pNoiseTexture);
    if (FAILED(hr)) return false;

    // 3. テクスチャをもとにSRVを定義・作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = pDevice->CreateShaderResourceView(m_pNoiseTexture, &srvDesc, &m_ssaoNoiseTextureSRV);
    if (FAILED(hr)) return false;

    return true;
}


bool Renderer::initSSAOSampler(ID3D11Device* pDevice) {

    HRESULT hr = S_OK;
    D3D11_SAMPLER_DESC sampDesc = {};

    // ---------------------------------------------------------
    // ① samPointClamp (点サンプリング / 範囲外クランプ)
    // ---------------------------------------------------------
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // Point指定
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;    // Clamp指定
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamPointClamp);
    if (FAILED(hr)) return false;

    // ---------------------------------------------------------
    // ② samPointWrap (点サンプリング / 範囲外ラップ・タイリング)
    // ---------------------------------------------------------
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // Point指定
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;     // Wrap指定 (タイリング用)
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamPointWrap);
    if (FAILED(hr)) return false;

    // ---------------------------------------------------------
    // ③ samLinearClamp (線形サンプリング / 範囲外クランプ)
    // ---------------------------------------------------------
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // Linear指定 (ぼかし用)
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;     // Clamp指定
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamLinearClamp);
    if (FAILED(hr)) return false;

    return true;
}