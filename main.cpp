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
#include"outLineMaterial.h"
#include "model.h"

Mesh* g_pCubeMesh = nullptr;
LitMaterial* g_pLitMaterial = nullptr; 
OutLineMaterial* g_pOutlineMaterial = nullptr; // 追加：アウトライン用マテリアル
Model* g_pMainModel = nullptr;
//追加
Model* g_pModel2 = nullptr;

// 定数バッファ
ID3D11Buffer* g_pConstantBuffer = nullptr;

//アウトラインクラス
#include "outLine.h"
OutLine* g_pOutLine = nullptr;

float g_Time = 0.0f;

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

    // Shader Manager
    g_pShaderManager = new ShaderManager();
    Shader* pLitShader = g_pShaderManager->GetOrCreate(pDevice, L"LitShader.hlsl");//Shadersフォルダに入れるのもいいか
    Shader* pOutlineShader = g_pShaderManager->GetOrCreate(pDevice, L"OutlineShader.hlsl");
    if (!pLitShader||!pOutlineShader) return false;

    // Mesh作成（Cube）
    g_pCubeMesh = Mesh::CreateCube(pDevice,1);

    // Material
    //litMaterial
    g_pLitMaterial = new LitMaterial();  // materialは実用できない。litにのみmBufferをもつ。
    UINT32 checker[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
    if (!g_pLitMaterial->Initialize(pDevice, pLitShader, checker, 2, 2))
        return false;
    g_pLitMaterial->CreateMaterialBuffer(pDevice);
    g_pLitMaterial->SetMaterialColor(0.8f, 0.6f, 0.2f, 1.0f);
    //outlienMaterial
	g_pOutlineMaterial = new OutLineMaterial();
	if (!g_pOutlineMaterial->Initialize(pDevice, pOutlineShader, checker, 2, 2))
		return false;
	g_pOutlineMaterial->CreateMaterialBuffer(pDevice);
	g_pOutlineMaterial->SetMaterialColor(1.0f, 0.0f, 0.0f, 1.0f); // 赤色で描画

    // Model
    g_pMainModel = new Model(pDevice, g_pCubeMesh, g_pLitMaterial);
    g_pMainModel->SetPosition(0.0f, 0.0f, 0.0f);
    //Model2
	g_pModel2 = new Model(pDevice, g_pCubeMesh, g_pLitMaterial);
	g_pModel2->SetPosition(0.0f, -1.0f, 5.0f);

    // --- 定数バッファの作成 ---
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerFrameCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    hr = pDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr)) return false;

    // Camera
    g_pCamera = new Camera(1280.0f, 720.0f);

    //outLine設定
	g_pOutLine = new OutLine();
	g_pOutLine->createStencilState(pDevice, g_pGraphics->GetContext());
    g_pOutLine->setMaterial(g_pOutlineMaterial);

    return true;
}

// =============================================
// 更新
void UpdateScene()
{
    g_Time += 0.016f;  // ≈60FPS

    // カメラ（自由に動かしたい場合は後でInput対応）
    XMVECTOR eye = XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_pCamera->Update(eye, at, up);

    // モデル回転
    g_pMainModel->SetRotation(0.0f, g_Time * 0.8f, 0.0f);
}

// =============================================
// 描画
void Render()
{
    ID3D11DeviceContext* pContext = g_pGraphics->GetContext();
    ID3D11Device* pDevice = g_pGraphics->GetDevice();
    g_pGraphics->BeginScene(0.1f, 0.12f, 0.15f, 1.0f);

    UpdateScene();

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ここでConstant Buffer更新（PerFrameCB）
    // パラメータの詰め込み
    PerFrameCB frameParams;
    // ↓★★★ これらが抜けているため、行列がゴミデータ（あるいは0）になっています！
    frameParams.matView = DirectX::XMMatrixTranspose(g_pCamera->GetViewMatrix());
    frameParams.matProjection = DirectX::XMMatrixTranspose(g_pCamera->GetProjectionMatrix());
    XMStoreFloat4(&frameParams.vLightPos, XMVectorSet(0.0f, 3.0f, 0.0f, 1.0f));
    frameParams.vLightColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    frameParams.vEyePos = g_pCamera->GetEyePosition();
    frameParams.vAttenuation = DirectX::XMFLOAT4(1.0f, 0.09f, 0.032f, 0.0f);
    
	pContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &frameParams, 0, 0);
	pContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

 	g_pOutLine->DrawOutline(pContext, g_pMainModel, g_pConstantBuffer);

    // 後処理（元のStateに戻す）
    pContext->OMSetDepthStencilState(g_pGraphics->m_pDefaultStencilState, 0);

    g_pGraphics->EndScene();
}

void CleanupDevice()
{
    if (g_pMainModel) { delete g_pMainModel;     g_pMainModel = nullptr; }
    if (g_pLitMaterial) { delete g_pLitMaterial;   g_pLitMaterial = nullptr; }
    if (g_pCubeMesh) { delete g_pCubeMesh;      g_pCubeMesh = nullptr; }
    if (g_pShaderManager) { delete g_pShaderManager; g_pShaderManager = nullptr; }
    if (g_pCamera) { delete g_pCamera;        g_pCamera = nullptr; }
    if (g_pGraphics) { delete g_pGraphics;      g_pGraphics = nullptr; }
}