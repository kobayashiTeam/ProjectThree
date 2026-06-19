// main.cpp
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// 基本システム
#include "graphics.h"
Graphics* g_pGraphics = nullptr;

// シェーダー管理
#include "ShaderManager.h"
ShaderManager* g_pShaderManager = nullptr;

// カメラ
#include "Camera.h"
Camera* g_pCamera = nullptr;

// モデル関連（Mesh + Material + Model）
#include "mesh.h"
#include "material.h"
#include "litMaterial.h"
#include"unLitMaterial.h"
#include"outLineMaterial.h"
#include "model.h"

Mesh* g_pCubeMesh = nullptr;
LitMaterial* g_pLitMaterial = nullptr; 
UnLitMaterial* g_pUnLitMaterial = nullptr; // 追加：UnLitMaterial
OutLineMaterial* g_pOutlineMaterial = nullptr; // 追加：アウトライン用マテリアル
Model* g_pMainModel = nullptr;
//追加
Model* g_pMainModel2 = nullptr;

// 定数バッファ
ID3D11Buffer* g_pConstantBuffer = nullptr;

//アウトラインクラス
#include "outLine.h"
OutLine* g_pOutLine = nullptr;

float g_Time = 0.0f;

//レンダーキュー
#include"renderQueue.h"

//計算にまつわるutilityクラスもinclude
#include"mathUtils.h"

//ラスタライザーステート
#include"rasterizerStates.h"

//深度ステンシルステート
#include"depthStencilStates.h"

//ブレンステート
#include"blendStates.h"

//レンダラークラス
#include"renderer.h"
Renderer* g_pRenderer = nullptr;

//共用クラス
#include"graphicsCommon.h"

//モデルリソース
#include"ModelResource.h"
ModelResource* modelResource = nullptr;

//input関連
// WndProcの上あたりに追加
bool g_keyLeft = false;
bool g_keyRight = false;
bool g_keyUp = false;
bool g_keyDown = false;

// 関数宣言
bool InitDevice();
void CleanupDevice();
void Render();
void UpdateScene();

// WndProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        // WndProc内に追加
    case WM_KEYDOWN:
        if (wParam == VK_LEFT)  g_keyLeft = true;
        if (wParam == VK_RIGHT) g_keyRight = true;
        if (wParam == VK_UP)    g_keyUp = true;
        if (wParam == VK_DOWN)  g_keyDown = true;
        return 0;
    case WM_KEYUP:
        if (wParam == VK_LEFT)  g_keyLeft = false;
        if (wParam == VK_RIGHT) g_keyRight = false;
        if (wParam == VK_UP)    g_keyUp = false;
        if (wParam == VK_DOWN)  g_keyDown = false;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"DX11_CleanBase";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"DirectX 11 - Clean Base",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 0;
    ShowWindow(hWnd, nCmdShow);

    // Graphics初期化
    g_pGraphics = new Graphics();
    if (!g_pGraphics->Initialize(hWnd, 1280, 720))
    {
        CleanupDevice();
        return 0;
    }

    if (!InitDevice())
    {
        CleanupDevice();
        return 0;
    }

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
            Render();
        }
    }

    CleanupDevice();
    return (int)msg.wParam;
}

// =============================================
// 初期化（ここをシンプルに保つ）
bool InitDevice()
{
    HRESULT hr;
    ID3D11Device* pDevice = g_pGraphics->GetDevice();

    if (!ShaderManager::GetInstance().LoadAllShaders(pDevice))
        return false;

    // Mesh作成（Cube）
    g_pCubeMesh = Mesh::CreateCube(pDevice,1);

    // Material
    //litMaterial
    g_pLitMaterial = new LitMaterial();  
    UINT32 checker[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
    if (!g_pLitMaterial->Initialize(pDevice, ShaderManager::GetInstance().GetShader(ShaderID::Lit)
        , checker, 2, 2))//lit
        return false;
    g_pLitMaterial->CreateMaterialBuffer(pDevice);
    g_pLitMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);//8,6,2,1
	//unLitMaterial
	g_pUnLitMaterial = new UnLitMaterial();
	if (!g_pUnLitMaterial->Initialize(pDevice, ShaderManager::GetInstance().GetShader(ShaderID::UnLit), 
        checker, 2, 2))//unlit
		return false;
	g_pUnLitMaterial->CreateMaterialBuffer(pDevice);
	g_pUnLitMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 0.3f); // 緑がかった色で描画
    //outlienMaterial
	g_pOutlineMaterial = new OutLineMaterial();
	if (!g_pOutlineMaterial->Initialize(pDevice, ShaderManager::GetInstance().GetShader(ShaderID::Outline), 
        checker, 2, 2))//outline
		return false;
	g_pOutlineMaterial->CreateMaterialBuffer(pDevice);
	g_pOutlineMaterial->SetMaterialColor(1.0f, 0.0f, 0.0f, 1.0f); // 赤色で描画

    // Model
    g_pMainModel = new Model(pDevice, g_pCubeMesh, g_pLitMaterial);
    g_pMainModel->SetPosition(0.0f, -1.0f, 5.0f);
    //Model2
	g_pMainModel2 = new Model(pDevice, g_pCubeMesh, g_pUnLitMaterial);
	g_pMainModel2->SetPosition(0.0f, 0.0f, 0.0f);
    g_pMainModel2->SetTransparent(true);
    //oldCamera(modelResource)
    modelResource->LoadFromFile(pDevice,nullptr,L"assets/oldCamera");

    // Camera
    g_pCamera = new Camera(1280.0f, 720.0f);

    //outLine設定
	g_pOutLine = new OutLine();
	g_pOutLine->createStencilState(pDevice, g_pGraphics->GetContext());
    g_pOutLine->setMaterial(g_pOutlineMaterial);

 	//レンダラー
	g_pRenderer = new Renderer();
    if (!g_pRenderer->Initialize(g_pGraphics)) {
        return false;
    }
    
    return true;
}

// =============================================
// 更新
void UpdateScene()
{
    g_Time += 0.016f;  // ≈60FPS

    // UpdateScene内のカメラ部分を置き換え
    const float rotSpeed = 0.02f;
    float deltaYaw = 0.0f;
    float deltaPitch = 0.0f;
    if (g_keyLeft)  deltaYaw += rotSpeed;
    if (g_keyRight) deltaYaw -= rotSpeed;
    if (g_keyUp)    deltaPitch += rotSpeed;
    if (g_keyDown)  deltaPitch -= rotSpeed;

    g_pCamera->UpdateDirection(deltaYaw, deltaPitch);

    // 以前の eye/at/up 渡しの3行は削除

    // モデル回転
    g_pMainModel->SetRotation(0.0f, g_Time * 0.8f, 0.0f);
}

// =============================================
// 描画
void Render()
{
    // 1. シーン全体の更新
    UpdateScene();

    // 2. 描画開始（クリア処理や定数バッファのセットを内部で自動化）
    g_pRenderer->BeginFrame(g_pCamera, 0.1f, 0.12f, 0.15f, 1.0f);

    // 3. モデルの登録（距離計算はRendererが裏で自動でやってくれる）
    g_pRenderer->Submit(g_pMainModel, RenderPass::Opaque,BlendMode::Opaque);
    g_pRenderer->Submit(g_pMainModel2, RenderPass::Transparent,BlendMode::AlphaBlend);

    // 4. レンダーキューの実行（適切なステートで一括描画）
    g_pRenderer->Execute();

    // 5. 描画終了（ポスト処理と表示）
    g_pRenderer->EndFrame();
}

void CleanupDevice()
{
    if (g_pMainModel) { delete g_pMainModel;     g_pMainModel = nullptr; }
    if (g_pLitMaterial) { delete g_pLitMaterial;   g_pLitMaterial = nullptr; }
    if (g_pCubeMesh) { delete g_pCubeMesh;      g_pCubeMesh = nullptr; }
    if (g_pCamera) { delete g_pCamera;        g_pCamera = nullptr; }
    if (g_pGraphics) { delete g_pGraphics;      g_pGraphics = nullptr; }
}