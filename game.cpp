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

    m_camera = new Camera((float)width, (float)height);

    m_renderer = new Renderer();
    if (!m_renderer->Initialize(m_graphics)) return false;

    m_textRenderer = new TextRenderer();
    if (!m_textRenderer->Initialize(pDevice, m_graphics->GetContext(),
        L"assets/fonts/DefaultFont.spritefont")) return false;

    //m_currentScene = std::make_unique<TestScene>();
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
    if (Input::IsKeyDown(VK_LEFT))  deltaYaw += rotSpeed;//(g_keyLeft)
    if (Input::IsKeyDown(VK_RIGHT)) deltaYaw -= rotSpeed;//right
    if (Input::IsKeyDown(VK_UP))    deltaPitch += rotSpeed;//up
    if (Input::IsKeyDown(VK_DOWN))  deltaPitch -= rotSpeed;//down
    m_camera->UpdateDirection(deltaYaw, deltaPitch);

    m_currentScene->Update(dt);

    //test
    Input::Update();

    if (IScene* next = m_currentScene->CheckTransition())
    {
        m_currentScene->Exit();
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
    m_currentScene->SubmitUI(m_textRenderer,dt);
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