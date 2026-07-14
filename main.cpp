// main.cpp
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cmath>
#include <DirectXPackedVector.h> // 必要に応じてインクルード 新規

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
#include"normalVizMaterial.h"
#include"normalMappingMaterial.h"
#include"parallaxMappingMaterial.h"
#include"deferredCBMaterial.h"
#include "model.h"

OutLineMaterial* g_pOutlineMaterial = nullptr; // 追加：アウトライン用マテリアル

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
#include"resourceManager.h"
ModelResource* modelResource = nullptr;

//ジオメトリクラス
//moveGS
#include"moveGSEffect.h"
//normalViz
#include"normalVizGSEffect.h"

//gameObject
#include"testScene.h"
TestScene g_scene;

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

    //より低レベルなパーツ
    if (!ShaderManager::GetInstance().LoadAllShaders(pDevice))
        return false;
    if (!ShaderManager::GetInstance().LoadAllGeometryShaders(pDevice)) {
        return false;
    }

    

    //比較的高レベルなパーツ
    // Mesh作成（Cube）

    // Material
    UINT32 checker[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
    UINT32 white = 0xFFFFFFFF;
    // HDR用の白（各チャンネル 1.0f の輝度）
    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
    
	    //outlienMaterial
	g_pOutlineMaterial = new OutLineMaterial();
	if (!g_pOutlineMaterial->Initialize(pDevice, ShaderManager::GetInstance().
        GetShader(ShaderID::Outline),  checker, 2, 2,true,false))return false;
	g_pOutlineMaterial->CreateMaterialBuffer(pDevice);
	g_pOutlineMaterial->SetMaterialColor(1.0f, 0.0f, 0.0f, 1.0f); // 赤色で描画
        
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

    //新規：g_scene
	g_scene.SetDevice(pDevice);
    g_scene.Enter();

    
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

    // モデル回転
    //g_pMainModel->SetRotation(0.0f, g_Time * 0.8f, 0.0f);

    // ★Sceneのゲームロジック更新はここで呼ぶ
    g_scene.Update(0.016f);//0.016
}

// =============================================
// 描画
void Render()
{
    // 1. シーン全体の更新
    UpdateScene();

    // 2. 描画開始（クリア処理や定数バッファのセットを内部で自動化）
    g_pRenderer->BeginFrame(g_pCamera, 0.1f, 0.12f, 0.15f, 1.0f);

    //★これまで6行あったSubmit列挙が1行に
    g_scene.Submit(g_pRenderer);

    // 4. レンダーキューの実行（適切なステートで一括描画）
    g_pRenderer->Execute();

    // 5. 描画終了（ポスト処理と表示）
    g_pRenderer->EndFrame();
}

void CleanupDevice()
{
    if (g_pCamera) { delete g_pCamera;        g_pCamera = nullptr; }
    if (g_pGraphics) { delete g_pGraphics;      g_pGraphics = nullptr; }
}