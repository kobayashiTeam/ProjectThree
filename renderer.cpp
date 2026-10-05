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
#include"tonemapPostProcess.h"
#include<random>
#include"postProcessChain.h"
#include"bloomPass.h"
#include"gBufferPass.h"
#include"ssaoPass.h"
#include"shadowSystem.h"
#include"deferredLightingPass.h"
#include"irradianceConvolutionPass.h"
#include"prefilterSpecularPass.h"
#include"gpuProfiler.h"

Renderer::~Renderer()
{
    // 所有権を持つマネジメントクラスの解放
    delete m_rasterStates;
    delete m_dsStates;
    delete m_blendStates;
    delete m_gpuProfiler;
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



    //simpleBlit
    m_finalRenderScreenBlitPostProcess = new ScreenBlitPostProcess();
    m_finalRenderScreenBlitPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::ScreenBlit));

    // Gバッファデバッグ表示用
    m_gBufferDebugBlit = new GBufferDebugBlit();
    m_gBufferDebugBlit->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::GBufferDebug));

    // Scene9用：metallic/roughness（アルファチャンネル）のグレースケール表示
    m_gBufferDebugAlphaBlit = new GBufferDebugBlit();
    m_gBufferDebugAlphaBlit->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::GBufferDebugAlpha));


    // HDR→LDRの境目（Bloom合成＋トーンマッピング）
    m_tonemapPostProcess = new TonemapPostProcess();
    m_tonemapPostProcess->Initialize(pDevice,
        ShaderManager::GetInstance().GetShader(ShaderID::Tonemap));

    // PostProcessChain の生成・初期化（すべてLDRの段＝トーンマッピングの後で実行される）
    // 並び順＝適用順：色を変える系 → ぼかし・シャープ → ビネット（画面の枠に関わるものは最後）
    m_postProcessChain = new PostProcessChain();
    m_postProcessChain->AddEffect<MonochromePostProcess>(pDevice, ShaderID::Monochromatic, false);
    m_postProcessChain->AddEffect<InversionPostProcess>(pDevice, ShaderID::Inversion, false);
    m_postProcessChain->AddEffect<SepiaPostProcess>(pDevice, ShaderID::Sepia, false);
    m_postProcessChain->AddEffect<SimpleBoxBlurPostProcess>(pDevice, ShaderID::SimpleBoxBlur, false);
    m_postProcessChain->AddEffect<SharpenPostProcess>(pDevice, ShaderID::Sharpen, false);
    m_vignette = m_postProcessChain->AddEffect<VignettePostProcess>(pDevice, ShaderID::Vignette, false);

    // Bloom（HDRの段）は縮小／拡大の連鎖を持つ専用パスとして独立クラスで管理
    m_bloomPass = new BloomPass();
    if (!m_bloomPass->Initialize(pDevice, 1280, 720)) return false;

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

    // IBL：スカイボックスからDiffuse Irradianceキューブマップを一度だけ焼き込む
    // （環境自体は変化しない前提の軽量IBLのため、毎フレームではなく起動時1回のみ実行）
    m_irradianceConvolutionPass = new IrradianceConvolutionPass();
    if (!m_irradianceConvolutionPass->Initialize(pDevice)) {
        return false;
    }
    m_irradianceConvolutionPass->Bake(pContext, m_pSkyBox->GetCubeMapSRV(), m_rasterStates, m_dsStates);

    // IBL：スカイボックスからSpecular Prefilterキューブマップ（roughnessごとのミップ付き）を焼き込む
    m_prefilterSpecularPass = new PrefilterSpecularPass();
    if (!m_prefilterSpecularPass->Initialize(pDevice)) {
        return false;
    }
    m_prefilterSpecularPass->Bake(pContext, m_pSkyBox->GetCubeMapSRV(), m_rasterStates, m_dsStates);

    //点をポリゴンに変えるクラスの生成、初期化
    m_pPointSpriteGSEffect = new PointSpriteGSEffect();
    if (!m_pPointSpriteGSEffect->Init(pDevice))return false;

    // InstancedModel の初期化
    m_pInstancedModel = new InstancedModel();
    if (!m_pInstancedModel->Init(pDevice, pContext, Mesh::CreateCube(pDevice, 1.0f), 512))return false;


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

    // GPUプロファイラ（タイムスタンプクエリ）
    m_gpuProfiler = new GpuProfiler();
    if (!m_gpuProfiler->Initialize(pDevice)) return false;

    return true;

}

void Renderer::BeginFrame(Camera* camera, float r, float g, float b, float a)
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    m_currentCamera = camera;

    // GPU計測の開始（フレーム先頭の時刻）。画面クリアもTotalに含めたいので BeginScene より前
    m_gpuProfiler->BeginFrame(pContext);

    m_graphics->BeginScene(r, g, b, a);

    // 共通のトポロジー設定
    m_graphics->GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // フレームごとの定数バッファ更新(b0)
    UpdatePerFrameConstantBuffer();
    // ※ライトの定数バッファ(b3/b4)の転送は Execute() の先頭へ移動した
    //   （BeginFrame → Submit → Execute の順なので、ここで送るとSubmitでの変更が1フレーム遅れて反映されるうえ、
    //     下の「毎フレーム既定値に戻す」設定は、Submitで変えた値が一度も送られなくなるため）

    // CSMは毎フレーム「無効」に戻し、使いたいシーンだけがSubmit()内で有効化する
    // （Rendererの状態がシーン遷移後に残り、他のシーンの見た目を変えてしまうのを防ぐ）
    m_shadowSystem->SetCascadeEnabled(false);
    m_shadowSystem->SetCascadeDebug(false);

    // ポストプロセスもCSMと同じく毎フレーム既定値に戻す（Scene6で変えた設定が他のシーンに残らないように）
    m_postProcessData.exposure = 0.5f;
    m_shadowSystem->SetPointLightIntensity(1.0f);
    m_bloomActive = true;
    m_bloomIntensity = 0.3f;
    m_tonemapper = Tonemapper::Exposure;
    SetVignetteActive(false);
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
    pContext->GSSetConstantBuffers(0, 1, cbArray);
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

    // ライトの定数バッファを送る（各シーンのSubmitでの変更がすべて終わった後）
    //DirectionalLightも共通なので送る(b3)
    m_shadowSystem->UpdateLightDataConstantBuffer(pContext);
    //PointLightも送る(b4)
    m_shadowSystem->UpdatePointLightConstantBuffer(pContext);

    int opaqueIdx = static_cast<int>(RenderPass::Opaque);
    int transparentIdx = static_cast<int>(RenderPass::Transparent);
    int deferredOpaqueIdx = static_cast<int>(RenderPass::DeferredOpaque);

    // ===== 1パス目：シャドウマップ生成 =====
    // ===== DirectionalLight のシャドウパス =====
    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::ShadowDir);
    m_shadowSystem->BeginDirectionalPass(pContext);  // 内部でm_shadowShader->Bind()済み
    m_renderQueues[opaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_renderQueues[deferredOpaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_shadowSystem->EndDirectionalPass(pContext);
    m_gpuProfiler->End(pContext, GpuProfiler::Section::ShadowDir);

    // ===== DirectionalLight のCSMパス（Deferred専用の新経路） =====
    // 行列の計算とb7への転送は毎フレーム行う（無効時も「無効」フラグを伝えるため）
    m_shadowSystem->UpdateCascades(pContext, m_currentCamera);
    if (m_shadowSystem->IsCascadeEnabled())
    {
        m_gpuProfiler->Begin(pContext, GpuProfiler::Section::ShadowCSM);
        // 段ごとに「描き込み先のスライス」と「b8の行列」だけを差し替えて、同じ物体を3回描く
        for (int c = 0; c < NUM_CASCADES; c++)
        {
            m_shadowSystem->BeginCascadePass(pContext, c);
            m_renderQueues[opaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
            m_renderQueues[deferredOpaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
            m_shadowSystem->EndCascadePass(pContext);
        }
        m_gpuProfiler->End(pContext, GpuProfiler::Section::ShadowCSM);
    }


    // ===== PointLight のシャドウパス =====
    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::ShadowPoint);
    m_shadowSystem->BeginPointPass(pContext);
    m_renderQueues[opaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_renderQueues[deferredOpaqueIdx].ExecuteGeometryOnly(pContext, m_perFrameCB.Get(), m_blendStates, false);
    m_shadowSystem->EndPointPass(pContext);
    m_gpuProfiler->End(pContext, GpuProfiler::Section::ShadowPoint);

    // ==========================================
    // 1. オフスクリーンRTに描画先を切り替え
    // ==========================================
    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::GBuffer); // オフスクリーンのクリアもここに含める
    m_offscreenRTwithMSAA->Clear(pContext);

    // ※以前はここで brightRT と2枚のMRTとしてバインドしていたが、Bloomの輝度抽出は
    //   BloomPassの最初の縮小で行うようになったので、書き込み先はシーンの1枚だけになった。
    //   （LitShader / DeferredLightingShader の SV_Target1 出力も削除済み）
    m_offscreenRTwithMSAA->Bind(pContext);


    //-----Lighting Pass-----
    m_shadowSystem->BindForLighting(pContext);  // シャドウマップ関連の設定

    //-----deferred不透明パス-----
    m_gBufferPass->Begin(pContext);             // G-Bufferのレンダーターゲットに切り替え

    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest);

    m_renderQueues[deferredOpaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates, true);
    // デバッグ表示の早期returnより前に閉じる（閉じ忘れると区間が記録されない）
    m_gpuProfiler->End(pContext, GpuProfiler::Section::GBuffer);

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
        case GBufferDebugView::Albedo:    debugSRV = m_gBufferPass->GetAlbedoSRV(); break;
        case GBufferDebugView::Normal:    debugSRV = m_gBufferPass->GetNormalSRV(); break;
        case GBufferDebugView::Depth:     debugSRV = m_gBufferPass->GetDepthSRV();  break;
            //  追加：metallicはAlbedoのa、roughnessはNormalのaに積んでいるので、
            //         同じSRVを「aだけ表示する」専用シェーダーに渡す
        case GBufferDebugView::Metallic:  debugSRV = m_gBufferPass->GetAlbedoSRV(); break;
        case GBufferDebugView::Roughness: debugSRV = m_gBufferPass->GetNormalSRV(); break;
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
        //  追加：Metallic/Roughnessはalphaチャンネルだけを見せる専用ブリットを使う
        else if (m_debugView == GBufferDebugView::Metallic || m_debugView == GBufferDebugView::Roughness)
        {
            m_gBufferDebugAlphaBlit->Render(pContext, debugSRV);
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

    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::SSAO);
    ID3D11ShaderResourceView* ssaoSRV = m_ssaoPass->Execute(
        pContext,
        m_gBufferPass->GetNormalSRV(),
        m_gBufferPass->GetPositionSRV(),
        m_finalRenderMesh);
    m_gpuProfiler->End(pContext, GpuProfiler::Section::SSAO);

    // オフスクリーンRTに再バインド（SSAOパスで描き込み先が変わっているため）
    m_offscreenRTwithMSAA->Bind(pContext);

    // ===== Lighting Pass =====
    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::Lighting);
    m_deferredLightingPass->Execute(pContext, m_gBufferPass, ssaoSRV, m_dsStates, m_finalRenderMesh,
        m_irradianceConvolutionPass->GetIrradianceSRV(),
        m_prefilterSpecularPass->GetPrefilterSRV());
    m_gpuProfiler->End(pContext, GpuProfiler::Section::Lighting);


    // ─── 工程1: 不透明パス ───
    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::Forward);
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest); // 通常の深度テスト
    m_renderQueues[opaqueIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates, true);


    // ─── InstancedModelの描画 ───
    //未設定時は早期returnされるため無害
    m_pInstancedModel->Render(pContext);

    // ─── スカイボックスの描画 ───
    if (m_pSkyBox) {
        m_rasterStates->Bind(pContext, RasterizerStates::CullMode::None);       // 前面カリングで内側を描画
        m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthLessEqual);    // DepthLessEqual で遠平面にフィット

        pContext->VSSetShader(nullptr, nullptr, 0);
        pContext->PSSetShader(nullptr, nullptr, 0);
        pContext->GSSetShader(nullptr, nullptr, 0);
        // 描画実行
        m_pSkyBox->Draw(pContext, m_currentCamera->GetViewMatrix(), m_currentCamera->GetProjectionMatrix());
    }


    // ─── 工程3: 半透明パス ───
    m_rasterStates->Bind(pContext, RasterizerStates::CullMode::Back);
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest);
    m_renderQueues[transparentIdx].Execute(pContext, m_perFrameCB.Get(), m_blendStates, true);



    // ==========================================
    // 3. MSAAの解除（Resolve）※計測上はForward区間に含める（Bloom区間と重ならないように）
    // ==========================================
    pContext->ResolveSubresource(
        m_offscreenRT->GetTexture(), 0,          // 転送先: 非MSAA RT
        m_offscreenRTwithMSAA->GetTexture(), 0,  // 転送元: MSAA RT
        DXGI_FORMAT_R16G16B16A16_FLOAT
    );
    m_gpuProfiler->End(pContext, GpuProfiler::Section::Forward);

    // b5（露出・トーンマッピング方式・Bloomの強さ・ガンマ）を更新してバインド
    UpdatePostProcessConstantBuffer();

    // ==========================================
    // 4. HDRの段：Bloom（縮小／拡大の連鎖）
    // ==========================================
    ID3D11ShaderResourceView* bloomSRV = nullptr;
    if (m_bloomActive) {
        m_gpuProfiler->Begin(pContext, GpuProfiler::Section::Bloom);
        bloomSRV = m_bloomPass->Execute(pContext, m_offscreenRT->GetSRV(), m_finalRenderMesh, m_blendStates);
        m_gpuProfiler->End(pContext, GpuProfiler::Section::Bloom);
    }

    // ==========================================
    // 5. HDR → LDR：Bloom合成＋トーンマッピング（offscreenRT → tmpRT）
    // ==========================================
    m_gpuProfiler->Begin(pContext, GpuProfiler::Section::Post);
    m_blendStates->Bind(pContext, BlendMode::Opaque);
    m_tmpRT->Clear(pContext);
    m_tmpRT->Bind(pContext); // ビューポートも1280x720に戻る
    m_tonemapPostProcess->SetBloomTexture(bloomSRV);
    m_tonemapPostProcess->Render(pContext, m_offscreenRT);
    m_finalRenderMesh->Render(pContext);
    m_tonemapPostProcess->Unbind(pContext);

    // ==========================================
    // 6. LDRの段：ポストプロセス・ピンポン・チェーン
    //    HDRの役目を終えた offscreenRT を、2枚目のピンポンバッファとして再利用する
    // ==========================================
    RenderTarget* finalResult = m_postProcessChain->Render(
        pContext, m_tmpRT, m_offscreenRT, m_finalRenderMesh);

    // ==========================================
    // 7. 出力先を「デフォルト（画面）」に戻して最終転写（ガンマ補正のみ）
    // ==========================================
    m_graphics->bindDefaultRenderTarget(); // バックバッファに切り替え
    m_blendStates->Bind(pContext, BlendMode::Opaque);

    m_finalRenderScreenBlitPostProcess->Render(pContext, finalResult);
    m_finalRenderMesh->Render(pContext);
    m_gpuProfiler->End(pContext, GpuProfiler::Section::Post);

    // t0 を外す（次フレームで offscreenRT / tmpRT をRTとして使うため）
    ID3D11ShaderResourceView* nullSRV = nullptr;
    pContext->PSSetShaderResources(0, 1, &nullSRV);
}

void Renderer::EndFrame()
{
    ID3D11DeviceContext* pContext = m_graphics->GetContext();

    // 後処理：デフォルトのステンシルステートなどに戻す
    m_dsStates->Bind(pContext, DepthStencilStates::Mode::DepthTest);

    // GPU計測の終了（フレーム末尾の時刻）。UIテキストまで含めたいので Present（EndScene）の直前
    m_gpuProfiler->EndFrame(pContext);

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
    postParams.tonemapper = (m_tonemapper == Tonemapper::ACES) ? 1.0f : 0.0f;
    postParams.bloomIntensity = m_bloomActive ? m_bloomIntensity : 0.0f;

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

void Renderer::SetPointLightIntensity(float intensity)
{
    if (m_shadowSystem) {
        m_shadowSystem->SetPointLightIntensity(intensity);
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

void Renderer::SetVignetteActive(bool isOn)
{
    if (m_vignette) m_vignette->SetActive(isOn);
}

void Renderer::SetInstanceCount(UINT count, DirectX::XMFLOAT3 offset)
{
    if (m_pInstancedModel) {
        m_pInstancedModel->SetActiveCount(count, offset);
    }
}

void Renderer::SetCascadedShadowEnabled(bool isOn)
{
    if (m_shadowSystem) m_shadowSystem->SetCascadeEnabled(isOn);
}

void Renderer::SetCascadeDebug(bool isOn)
{
    if (m_shadowSystem) m_shadowSystem->SetCascadeDebug(isOn);
}

void Renderer::SetCascadeLambda(float lambda)
{
    if (m_shadowSystem) m_shadowSystem->SetCascadeLambda(lambda);
}

float Renderer::GetCascadeSplit(int index) const
{
    return m_shadowSystem ? m_shadowSystem->GetCascadeSplit(index) : 0.0f;
}
