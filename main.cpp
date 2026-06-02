// main.cpp
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

#include "graphics.h"
Graphics* g_pGraphics = nullptr;

// ↓ シェーダー・テクスチャ・サンプラーの個別グローバル変数はすべて削除
// （MaterialクラスとMeshクラスに移譲済み）

#include "material.h"
#include"litMaterial.h"
LitMaterial* g_pLitMaterial = nullptr;
Material* g_pUnlitMaterial = nullptr;

// ↓ ★【追加】シェーダーマネージャーのインクルードとグローバル変数
#include "ShaderManager.h"
ShaderManager* g_pShaderManager = nullptr;

#include "vertex.h"
#include"model.h"


ID3D11Buffer* g_pConstantBuffer = nullptr;
float g_Time = 0.0f;

#include "Camera.h"
Camera* g_pCamera = nullptr;
#include "mesh.h"
Mesh* g_pCubeMesh = nullptr;
#include "graphicsCommon.h"

// ★【進化ポイント】ゲーム上のオブジェクトは「Model」として管理！
Model* g_pMainCubeInstance = nullptr;
Model* g_pLightCubeInstance = nullptr;

bool InitDevice(HWND hWnd);
void CleanupDevice();
void Render();

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
    const wchar_t CLASS_NAME[] = L"MyGameWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(
        0, CLASS_NAME, L"DirectX 11 - Engine Refactoring",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr
    );

    if (hWnd == nullptr) return 0;

    ShowWindow(hWnd, nCmdShow);

    g_pGraphics = new Graphics();
    if (!g_pGraphics->Initialize(hWnd, 800, 600))
    {
        CleanupDevice();
        return 0;
    }

    g_pCubeMesh = new Mesh();

    if (!InitDevice(hWnd))
    {
        CleanupDevice();
        return 0;
    }

    g_pCamera = new Camera(800.0f, 600.0f);

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

bool InitDevice(HWND hWnd)
{
    ID3D11Device* pDevice = g_pGraphics->GetDevice();
    HRESULT hr;

    // --- メッシュデータ ---
    SimpleVertex vertices[] =
    {
        { -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        {  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
    };

    DWORD indices[] =
    {
        0, 1, 2,    0, 2, 3,
        4, 5, 6,    4, 6, 7,
        8, 9, 10,   8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };

    UINT vertexCount = sizeof(vertices) / sizeof(SimpleVertex);
    UINT indexCount = sizeof(indices) / sizeof(DWORD);

    if (!g_pCubeMesh->Create(pDevice, vertices, vertexCount, indices, indexCount))
        return false;

    // --- マテリアルの生成と初期化 ---
    // ↓ pixels はここで1回だけ宣言する
    UINT32 pixels[4] = {
        0xFFFFFFFF, 0xFF000000,
        0xFF000000, 0xFFFFFFFF
    };

    // --- ★【変更】シェーダーマネージャーの生成とシェーダーの取得 ---
    g_pShaderManager = new ShaderManager();
    Shader* pLitShader = g_pShaderManager->GetOrCreate(pDevice, L"LitShader.hlsl");
    Shader* pUnlitShader = g_pShaderManager->GetOrCreate(pDevice, L"UnlitShader.hlsl");

    if (!pLitShader||!pUnlitShader) return false;

    // --- マテリアルの生成と初期化 ---
	g_pLitMaterial = new LitMaterial();
	g_pUnlitMaterial = new Material();
    // 引数にファイル名ではなく、取得した pShader を渡すように変更
    if ((!g_pLitMaterial->Initialize(pDevice, pLitShader, pixels, 2, 2))||
        (!g_pUnlitMaterial->Initialize(pDevice, pUnlitShader, pixels, 2, 2)))
        return false;
    g_pLitMaterial->CreateMaterialBuffer(pDevice); // 専用バッファ作成
    g_pLitMaterial->SetMaterialColor(1.0f, 0.5f, 0.5f, 1.0f); // 例えばちょっと赤っぽくしてみる


    // --- 定数バッファの作成 ---
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerFrameCB);//ConstantBufferParameters
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    hr = pDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr)) return false;

    // ↑ テクスチャ・シェーダー・サンプラーの個別作成コードは
    //   すべてMaterial::Initialize内で処理されるため削除

    // ★【進化ポイント】Modelインスタンスの生成と初期配置
    // 同じ g_pCubeMesh と g_pCubeMaterial を2つのモデルで「共有」している点に注目してください！
    // 第1引数に pDevice を追加
    g_pMainCubeInstance = new Model(pDevice, g_pCubeMesh, g_pLitMaterial);
    g_pMainCubeInstance->SetPosition(0.0f, 0.0f, 0.0f);

    g_pLightCubeInstance = new Model(pDevice, g_pCubeMesh, g_pUnlitMaterial);
    g_pLightCubeInstance->SetScale(0.1f, 0.1f, 0.1f);

    return true;
}

void Render()
{
    g_Time += 0.01f;
    ID3D11DeviceContext* pContext = g_pGraphics->GetContext();
    g_pGraphics->BeginScene(0.1f, 0.1f, 0.1f, 1.0f);

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // カメラの更新
    XMVECTOR eye = XMVectorSet(0.0f, 2.0f, -4.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_pCamera->Update(eye, at, up);

    // ライトの位置計算
    float lightRadius = 1.5f;
    float lightX = sinf(g_Time * 2.0f) * lightRadius;
    float lightZ = cosf(g_Time * 2.0f) * lightRadius;
    float lightY = 0.5f;

    // 各モデルを更新（main側で行列を直に計算しなくてよくなりました）
    g_pMainCubeInstance->SetRotation(0.0f, g_Time, 0.0f); // Y軸回転
    g_pLightCubeInstance->SetPosition(lightX, lightY, lightZ); // ライトの位置へ追従

    // パラメータの詰め込み
    PerFrameCB frameParams;
    // ↓★★★ これらが抜けているため、行列がゴミデータ（あるいは0）になっています！
    frameParams.matView = DirectX::XMMatrixTranspose(g_pCamera->GetViewMatrix());
    frameParams.matProjection = DirectX::XMMatrixTranspose(g_pCamera->GetProjectionMatrix());
    XMStoreFloat4(&frameParams.vLightPos, XMVectorSet(lightX, lightY, lightZ, 1.0f));
    frameParams.vLightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    frameParams.vEyePos = g_pCamera->GetEyePosition();
    frameParams.vAttenuation = XMFLOAT4(1.0f, 0.09f, 0.032f, 0.0f);

    // 共通のグローバルバッファ（スロット0用）に書き込み
    pContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &frameParams, 0, 0);

    // ★【進化ポイント】それぞれのインスタンスに「描画して！」と命令するだけ
    // --- 1. メインキューブの描画 ---
    g_pMainCubeInstance->Draw(pContext, g_pConstantBuffer);

    // --- 2. 電球キューブの描画 ---
    // 電球自体は発光しているように見せたいので、ライトカラーのアルファ(w)を0にして区別していた元の仕様を適用
    frameParams.vLightColor.w = 0.0f;
    g_pLightCubeInstance->Draw(pContext, g_pConstantBuffer);

    g_pGraphics->EndScene();
}

void CleanupDevice()
{
    // Modelインスタンスの解放
    if (g_pMainCubeInstance) { delete g_pMainCubeInstance;  g_pMainCubeInstance = nullptr; }
    if (g_pLightCubeInstance) { delete g_pLightCubeInstance; g_pLightCubeInstance = nullptr; }

    // アセットの解放
    if (g_pLitMaterial) { delete g_pLitMaterial;   g_pLitMaterial = nullptr; }
    if (g_pUnlitMaterial) { delete g_pUnlitMaterial;   g_pUnlitMaterial = nullptr; }

    // --- ★【追加】シェーダーマネージャーの解放 ---
    if (g_pShaderManager) { delete g_pShaderManager; g_pShaderManager = nullptr; }

    if (g_pCubeMesh) { delete g_pCubeMesh;       g_pCubeMesh = nullptr; }

    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pCamera) { delete g_pCamera;         g_pCamera = nullptr; }
    if (g_pGraphics) { delete g_pGraphics;       g_pGraphics = nullptr; }
}