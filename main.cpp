// main.cpp
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// 【新規追加】グラフィックスコアクラスのインクルード
#include "graphics.h"
Graphics* g_pGraphics = nullptr;

// ---------------------------------------------------------
// 全局変数（シェーダーやテクスチャなど、メイン側に残るオブジェクト）
// ---------------------------------------------------------
// ※デバイスやスワップチェーン、深度バッファ、ラスタライザは Graphics クラスへ移動したため削除しました

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11ShaderResourceView* g_pTextureRV = nullptr;
ID3D11SamplerState* g_pSamplerLinear = nullptr;

#include "vertex.h"

struct ConstantBuffer
{
    XMMATRIX mModel;
    XMMATRIX mView;
    XMMATRIX mProjection;
    XMFLOAT4 vLightPos;
    XMFLOAT4 vLightColor;
    XMFLOAT4 vEyePos;
    XMFLOAT4 vAttenuation;
};

ID3D11Buffer* g_pConstantBuffer = nullptr;
float g_Time = 0.0f;

#include "Camera.h"
Camera* g_pCamera = nullptr;
#include "mesh.h"
Mesh* g_pCubeMesh = nullptr;

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

    // 【変更】最初にGraphicsクラスを生成
    g_pGraphics = new Graphics();
    if (!g_pGraphics->Initialize(hWnd, 800, 600))
    {
        CleanupDevice();
        return 0;
    }

    g_pCubeMesh = new Mesh();

    // シェーダーやリソース類の初期化（引数として生成済みのデバイスを渡すよう変更可能ですが、
    // 今はInitDevice内でg_pGraphicsから取得します）
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

// ---------------------------------------------------------
// デバイスに依存するリソースの初期化関数
// ---------------------------------------------------------
bool InitDevice(HWND hWnd)
{
    // 【重要】GraphicsクラスからDeviceを取得する
    ID3D11Device* pDevice = g_pGraphics->GetDevice();

    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    // 頂点シェーダーのコンパイル
    hr = D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    // 【変更】pDevice を使用
    hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr)) return false;

    // ピクセルシェーダーのコンパイル
    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompileFromFile(L"Shader.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &pPSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    // 【変更】pDevice を使用
    hr = pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return false;

    // 頂点レイアウトの作成
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 9, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // 【変更】pDevice を使用
    hr = pDevice->CreateInputLayout(layout, 4, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr)) return false;

    // メッシュデータの定義
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

    // 【変更】pDevice を使用
    if (!g_pCubeMesh->Create(pDevice, vertices, vertexCount, indices, indexCount))
    {
        return false;
    }

    // 定数バッファの作成
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    // 【変更】pDevice を使用
    hr = pDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr)) return false;

    // テクスチャの作成
    UINT32 pixels[4] = {
        0xFFFFFFFF, 0xFF000000,
        0xFF000000, 0xFFFFFFFF
    };

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 2;
    td.Height = 2;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA tInitData = {};
    tInitData.pSysMem = pixels;
    tInitData.SysMemPitch = 2 * sizeof(UINT32);

    ID3D11Texture2D* pTexture2D = nullptr;
    // 【変更】pDevice を使用
    hr = pDevice->CreateTexture2D(&td, &tInitData, &pTexture2D);
    if (FAILED(hr)) return false;

    // 【変更】pDevice を使用
    hr = pDevice->CreateShaderResourceView(pTexture2D, nullptr, &g_pTextureRV);
    pTexture2D->Release();
    if (FAILED(hr)) return false;

    // サンプラーの作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    // 【変更】pDevice を使用
    hr = pDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
    if (FAILED(hr)) return false;

    return true;
}

// ---------------------------------------------------------
// 毎フレームの描画関数
// ---------------------------------------------------------
void Render()
{
    g_Time += 0.01f;

    // 【重要】GraphicsクラスからContextを取得する
    ID3D11DeviceContext* pContext = g_pGraphics->GetContext();

    // 【変更】画面クリアをGraphicsクラスに任せる
    g_pGraphics->BeginScene(0.1f, 0.1f, 0.1f, 1.0f);

    // 【変更】以降、g_pImmediateContext だった部分を pContext に差し替え
    pContext->IASetInputLayout(g_pVertexLayout);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    pContext->VSSetShader(g_pVertexShader, nullptr, 0);
    pContext->PSSetShader(g_pPixelShader, nullptr, 0);

    pContext->PSSetShaderResources(0, 1, &g_pTextureRV);
    pContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    // カメラ・プロジェクション計算
    XMVECTOR eye = XMVectorSet(0.0f, 2.0f, -4.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_pCamera->Update(eye, at, up);

    // ライト位置
    float lightRadius = 1.5f;
    float lightX = sinf(g_Time * 2.0f) * lightRadius;
    float lightZ = cosf(g_Time * 2.0f) * lightRadius;
    float lightY = 0.5f;

    // --- 1回目：メインキューブ ---
    XMMATRIX mModel = XMMatrixRotationY(g_Time);

    ConstantBuffer cb;
    cb.mModel = XMMatrixTranspose(mModel);
    cb.mView = XMMatrixTranspose(g_pCamera->GetViewMatrix());
    cb.mProjection = XMMatrixTranspose(g_pCamera->GetProjectionMatrix());
    XMStoreFloat4(&cb.vLightPos, XMVectorSet(lightX, lightY, lightZ, 1.0f));
    cb.vLightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cb.vEyePos = g_pCamera->GetEyePosition();
    cb.vAttenuation = XMFLOAT4(1.0f, 0.09f, 0.032f, 0.0f);

    pContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
    pContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    pContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pCubeMesh->Render(pContext); // Mesh側もContextを受け取る

    // --- 2回目：電球キューブ ---
    XMMATRIX mLightModel = XMMatrixScaling(0.1f, 0.1f, 0.1f) * 
        XMMatrixTranslation(lightX, lightY, lightZ);
    cb.mModel = XMMatrixTranspose(mLightModel);
    cb.vLightColor.w = 0.0f; // ライト計算スキップフラグ

    pContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
    g_pCubeMesh->Render(pContext);

    // 【変更】スワップチェーンのPresentをGraphicsクラスに任せる
    g_pGraphics->EndScene();
}

// ---------------------------------------------------------
// 後片付け関数
// ---------------------------------------------------------
void CleanupDevice()
{
    // メイン側に残ったリソースの解放
    if (g_pVertexLayout)    g_pVertexLayout->Release();
    if (g_pPixelShader)     g_pPixelShader->Release();
    if (g_pVertexShader)    g_pVertexShader->Release();
    if (g_pConstantBuffer)  g_pConstantBuffer->Release();
    if (g_pSamplerLinear)   g_pSamplerLinear->Release();
    if (g_pTextureRV)       g_pTextureRV->Release();

    if (g_pCamera) { delete g_pCamera;   g_pCamera = nullptr; }
    if (g_pCubeMesh) { delete g_pCubeMesh; g_pCubeMesh = nullptr; }

    // 【新規追加】Graphicsクラスの解放
    if (g_pGraphics) { delete g_pGraphics; g_pGraphics = nullptr; }
}