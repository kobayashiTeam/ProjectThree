// game.cpp
#include "game.h"
#include "graphics.h"
#include "ShaderManager.h"
#include "Camera.h"
#include "renderer.h"
#include "iScene.h"
#include "testScene.h"
#include"scene0.h"
#include"textRenderer.h"
#include"input.h"
#include"gpuProfiler.h"
#include<cstdio> // swprintf_s

// main.cppのWndProcが更新するグローバル入力状態を、今はそのまま参照する
extern bool g_keyLeft;
extern bool g_keyRight;
extern bool g_keyUp;
extern bool g_keyDown;

bool Game::Initialize(HWND hWnd, UINT width, UINT height)
{
    m_graphics = new Graphics();
    if (!m_graphics->Initialize(hWnd, width, height)) return false;

    ID3D11Device* pDevice = m_graphics->GetDevice();
    if (!ShaderManager::GetInstance().LoadAllShaders(pDevice)) return false;
    if (!ShaderManager::GetInstance().LoadAllGeometryShaders(pDevice)) return false;
    if (!ShaderManager::GetInstance().LoadAllComputeShaders(pDevice)) return false;

    m_camera = new Camera((float)width, (float)height);

    m_renderer = new Renderer();
    if (!m_renderer->Initialize(m_graphics)) return false;

    m_textRenderer = new TextRenderer();
    if (!m_textRenderer->Initialize(pDevice, m_graphics->GetContext(),
        L"assets/fonts/DefaultFont.spritefont")) return false;

    m_currentScene = std::make_unique<Scene0>();
    m_currentScene->SetDevice(pDevice);
    if (!m_currentScene->Enter()) return false;

    return true;
}

void Game::Run()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Update(0.016f);
            Render(0.016f);
        }
    }
}

void Game::Update(float dt)
{
    const float rotSpeed = 0.02f;
    float deltaYaw = 0.0f, deltaPitch = 0.0f;
    if (Input::IsKeyDown(VK_LEFT))  deltaYaw += rotSpeed;
    if (Input::IsKeyDown(VK_RIGHT)) deltaYaw -= rotSpeed;
    if (Input::IsKeyDown(VK_UP))    deltaPitch += rotSpeed;
    if (Input::IsKeyDown(VK_DOWN))  deltaPitch -= rotSpeed;
    m_camera->UpdateDirection(deltaYaw, deltaPitch);

    m_currentScene->Update(dt);

    // F3：GPU計測の表示ON/OFF（全シーン共通）
    if (Input::IsKeyPressed(VK_F3)) {
        if (GpuProfiler* profiler = m_renderer->GetGpuProfiler()) profiler->ToggleVisible();
    }
    // F5：視錐台カリングのON/OFF（全シーン共通。計測で見比べる用）
    if (Input::IsKeyPressed(VK_F5)) {
        m_renderer->ToggleFrustumCulling();
    }
    // F4：垂直同期のON/OFF（計測時はOFFにしてGPUのクロックを安定させる）
    if (Input::IsKeyPressed(VK_F4)) {
        m_graphics->ToggleVSync();
    }
    // F7：SSAOを全解像度／半分の解像度で切り替え（全シーン共通。計測区間 SSAO で見比べる）
    if (Input::IsKeyPressed(VK_F7)) {
        m_renderer->ToggleSSAOHalfRes();
    }

    Input::Update();

    if (IScene* next = m_currentScene->CheckTransition())
    {
        m_currentScene->Exit(m_renderer);
        next->SetDevice(m_graphics->GetDevice());
        next->Enter();
        m_currentScene.reset(next);
    }
}

void Game::Render(float dt)
{
    m_renderer->BeginFrame(m_camera, 0.1f, 0.12f, 0.15f, 1.0f);
    m_currentScene->Submit(m_renderer);
    m_renderer->Execute();

    // UIテキストは3D描画がすべて終わった後、バックバッファに直接重ね描きする
    m_textRenderer->Begin();
    m_currentScene->SubmitUI(m_textRenderer, dt);
    // GPU計測の結果（画面右上）。シーンのUIの上に重ねる
    if (GpuProfiler* profiler = m_renderer->GetGpuProfiler()) {
        // 表の位置は画面幅から決める（解像度の持ち主は Graphics）
        const float screenW = static_cast<float>(m_graphics->GetWidth());
        profiler->DrawOverlay(m_textRenderer, screenW);

        // 視錐台カリングの状態と「描いた数／登録数」（GPU計測の見出しのすぐ上）
        if (profiler->IsVisible()) {
            const RenderQueue::CullStats& cs = m_renderer->GetCullStats();
            const bool on = m_renderer->IsFrustumCullingEnabled();
            const float left = GpuProfiler::OverlayLeft(screenW); // 表の左端にそろえる
            wchar_t buf[64];
            swprintf_s(buf, L"Cull %s  %d/%d  F5", on ? L"ON " : L"OFF", cs.drawn, cs.total);
            m_textRenderer->DrawString(buf, left, 40.0f,
                on ? 0.5f : 1.0f, on ? 1.0f : 0.6f, on ? 0.5f : 0.3f, 1.0f, 0.6f);
            const bool vs = m_graphics->IsVSyncEnabled();
            m_textRenderer->DrawString(vs ? L"VSync ON   F4" : L"VSync OFF  F4",
                left, 22.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.6f);
            // SSAOの解像度（VSyncの行のさらに上）
            const bool half = m_renderer->IsSSAOHalfRes();
            m_textRenderer->DrawString(half ? L"SSAO Half  F7" : L"SSAO Full  F7",
                left, 4.0f, half ? 0.5f : 1.0f, 1.0f, half ? 0.5f : 1.0f, 1.0f, 0.6f);
        }
    }
    m_textRenderer->End();

    m_renderer->EndFrame();
}

void Game::Shutdown()
{
    if (m_camera) { delete m_camera;   m_camera = nullptr; }
    if (m_textRenderer) {
        delete m_textRenderer; m_textRenderer = nullptr;
    }
    if (m_renderer) { delete m_renderer; m_renderer = nullptr; }
    if (m_graphics) { delete m_graphics; m_graphics = nullptr; }
}
