#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h> // DirectXTKのWICローダー
#include <DirectXMath.h>
#include <array>
#include <string>

// ライブラリのリンク
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

//--------------------------------------------------------------------------------------
// 定数バッファの構造体
//--------------------------------------------------------------------------------------
struct ConstantBuffer
{
    XMMATRIX WVP; // World * View * Projection
};

//--------------------------------------------------------------------------------------
// 頂点構造体
//--------------------------------------------------------------------------------------
struct Vertex
{
    XMFLOAT3 Pos;
};

//--------------------------------------------------------------------------------------
// HLSL シェーダーコード（文字列として埋め込み）
//--------------------------------------------------------------------------------------
const char* g_shaderCode = R"(
cbuffer ConstantBuffer : register(b0)
{
    matrix WVP;
};

TextureCube g_SkyboxTex : register(t0);
SamplerState g_SamLinear : register(s0);

struct VS_INPUT
{
    float3 Pos : POSITION;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    
    // 頂点位置を変換
    output.Pos = mul(float4(input.Pos, 1.0f), WVP);
    
    // 【重要】深度値を強制的に最奥(1.0)にするトリック
    // ラスタライズ後の z/w が 1.0 になるよう、z に w を代入する
    output.Pos.z = output.Pos.w;
    
    // 立方体のローカル座標をそのままキューブマップのサンプリングベクトルとして使用
    output.TexCoord = input.Pos;
    
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    // キューブマップをサンプリング
    return g_SkyboxTex.Sample(g_SamLinear, input.TexCoord);
}
)";

//--------------------------------------------------------------------------------------
// グローバル変数
//--------------------------------------------------------------------------------------
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_HARDWARE;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;

// スカイボックス用リソース
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;
ID3D11Buffer* g_pConstantBuffer = nullptr;
ID3D11ShaderResourceView* g_pSkyboxSRV = nullptr;
ID3D11SamplerState* g_pSamplerLinear = nullptr;
ID3D11RasterizerState* g_pRasterizerNone = nullptr;
ID3D11DepthStencilState* g_pDepthStencilLessEqual = nullptr;

// カメラ制御用変数
float g_cameraYaw = 0.0f;
float g_cameraPitch = 0.0f;

//--------------------------------------------------------------------------------------
// 前方宣言
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void Render();

//--------------------------------------------------------------------------------------
// 6枚のPNGからキューブマップSRVを作成する関数 (ご提示のシグネチャを参考)
//--------------------------------------------------------------------------------------
bool CreateSkyboxSRV(ID3D11Device* pDevice, const std::array<std::wstring, 6>& faces, ID3D11ShaderResourceView** ppSRV)
{
    HRESULT hr = S_OK;
    std::array<ID3D11Texture2D*, 6> srcTextures = { nullptr };

    // 1. 6枚の画像を個別に読み込む
    for (int i = 0; i < 6; ++i)
    {
        ID3D11Resource* res = nullptr;
        hr = CreateWICTextureFromFile(pDevice, faces[i].c_str(), &res, nullptr);
        if (FAILED(hr))
        {
            MessageBoxA(nullptr, "PNGテクスチャの読み込みに失敗しました。パスを確認してください。", "Error", MB_OK);
            return false;
        }
        hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&srcTextures[i]);
        res->Release();
        if (FAILED(hr)) return false;
    }

    // 最初のテクスチャから情報を取得
    D3D11_TEXTURE2D_DESC srcDesc;
    srcTextures[0]->GetDesc(&srcDesc);

    // 2. キューブマップ用の Texture2D 記述子を設定
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = srcDesc.Width;
    texDesc.Height = srcDesc.Height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 6; // 6面分
    texDesc.Format = srcDesc.Format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // 【重要】キューブマップ指定

    ID3D11Texture2D* pCubeTexture = nullptr;
    hr = pDevice->CreateTexture2D(&texDesc, nullptr, &pCubeTexture);
    if (FAILED(hr)) return false;

    // 3. 読み込んだ6枚のデータをキューブマップの各配列要素にコピー
    for (int i = 0; i < 6; ++i)
    {
        // D3D11CalcSubresource(MipIndex, ArrayIndex, MipLevels)
        UINT subresourceIndex = D3D11CalcSubresource(0, i, 1);
        g_pImmediateContext->CopySubresourceRegion(pCubeTexture, subresourceIndex, 0, 0, 0, srcTextures[i], 0, nullptr);
        srcTextures[i]->Release(); // 不要になった個別テクスチャを解放
    }

    // 4. SRV（シェーダー・リソース・ビュー）の作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // クラスをTextureCubeとして見せる
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    hr = pDevice->CreateShaderResourceView(pCubeTexture, &srvDesc, ppSRV);
    pCubeTexture->Release();

    return SUCCEEDED(hr);
}

//--------------------------------------------------------------------------------------
// エントリーポイント
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (FAILED(InitWindow(hInstance, nCmdShow)))
        return 0;

    if (FAILED(InitDevice()))
    {
        CleanupDevice();
        return 0;
    }

    // メイン ループ
    MSG msg = { 0 };
    while (WM_QUIT != msg.message)
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

//--------------------------------------------------------------------------------------
// ウィンドウの初期化
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"SkyboxWindowClass";
    if (!RegisterClassEx(&wcex))
        return E_FAIL;

    g_hInst = hInstance;
    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindow(L"SkyboxWindowClass", L"Direct3D 11 Skybox Solo Test (Arrow Keys to Rotate)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd)
        return E_FAIL;

    ShowWindow(g_hWnd, nCmdShow);
    return S_OK;
}

//--------------------------------------------------------------------------------------
// D3D11デバイスとスカイボックスリソースの初期化
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    hr = D3D11CreateDeviceAndSwapChain(nullptr, g_driverType, nullptr, createDeviceFlags, nullptr, 0,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
    if (FAILED(hr)) return hr;

    // レンダーターゲットビュー作成
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr)) return hr;
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return hr;

    // 深度ステンシルビュー作成
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
    if (FAILED(hr)) return hr;

    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr)) return hr;

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // ビューポート設定
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // ----------------------------------------------------------------------------------
    // シェーダーのインラインコンパイル
    // ----------------------------------------------------------------------------------
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        if (pErrorBlob) {
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return hr;
    }
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr)) return hr;

    // 入力レイアウト定義
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_pd3dDevice->CreateInputLayout(layout, 1, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr)) return hr;

    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompile(g_shaderCode, strlen(g_shaderCode), nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &pPSBlob, &pErrorBlob);
    if (FAILED(hr)) return hr;
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return hr;

    // ----------------------------------------------------------------------------------
    // 立方体頂点・インデックスバッファの作成
    // ----------------------------------------------------------------------------------
    // スカイボックスのトレイリング(内側表示)のため、1x1x1の標準的な立方体
    Vertex vertices[] = {
        { XMFLOAT3(-1.0f,  1.0f, -1.0f) }, { XMFLOAT3(1.0f,  1.0f, -1.0f) },
        { XMFLOAT3(1.0f, -1.0f, -1.0f) }, { XMFLOAT3(-1.0f, -1.0f, -1.0f) },
        { XMFLOAT3(-1.0f,  1.0f,  1.0f) }, { XMFLOAT3(1.0f,  1.0f,  1.0f) },
        { XMFLOAT3(1.0f, -1.0f,  1.0f) }, { XMFLOAT3(-1.0f, -1.0f,  1.0f) },
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr)) return hr;

    // インデックス (内側から見えるようにワインディング)
    WORD indices[] = {
        0, 1, 2,  2, 3, 0, // 前
        4, 5, 1,  1, 0, 4, // 上
        3, 2, 6,  6, 7, 3, // 下
        1, 5, 6,  6, 2, 1, // 右
        4, 0, 3,  3, 7, 4, // 左
        5, 4, 7,  7, 6, 5  // 後
    };
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    InitData.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
    if (FAILED(hr)) return hr;

    // 定数バッファ作成
    bd.ByteWidth = sizeof(ConstantBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pConstantBuffer);
    if (FAILED(hr)) return hr;

    // サンプラーステート作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
    if (FAILED(hr)) return hr;

    // ----------------------------------------------------------------------------------
    // パイプラインステート（カリング無効 & 深度テスト LessEqual）の設定
    // ----------------------------------------------------------------------------------
    // カリングをNONEに（内側・外側両方描画されるようにして安全性を確保）
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    hr = g_pd3dDevice->CreateRasterizerState(&rasterDesc, &g_pRasterizerNone);
    if (FAILED(hr)) return hr;

    // 深度テスト関数を LESS_EQUAL に。これにより、すでに不透明オブジェクトが
    // 描画されて深度が 1.0 になっている背景部分にのみスカイボックスが滑り込める
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // ここが肝
    hr = g_pd3dDevice->CreateDepthStencilState(&dsDesc, &g_pDepthStencilLessEqual);
    if (FAILED(hr)) return hr;

    // ----------------------------------------------------------------------------------
    // テクスチャロード部 (ご指定のパス配列を使用)
    // ----------------------------------------------------------------------------------
    std::array<std::wstring, 6> skyboxFaces = {
        L"assets/skybox/red.png",    // [0] +X
        L"assets/skybox/green.png",  // [1] -X
        L"assets/skybox/blue.png",   // [2] +Y
        L"assets/skybox/yellow.png", // [3] -Y
        L"assets/skybox/white.png",  // [4] +Z
        L"assets/skybox/purple.png", // [5] -Z
    };

    //std::array<std::wstring, 6> skyboxFaces = {
    //    L"assets/skybox/vz_dawn_right.png",    // [0] +X
    //    L"assets/skybox/vz_dawn_left.png",  // [1] -X
    //    L"assets/skybox/vz_dawn_up.png",   // [2] +Y
    //    L"assets/skybox/vz_dawn_down.png", // [3] -Y
    //    L"assets/skybox/vz_dawn_front.png",  // [4] +Z
    //    L"assets/skybox/vz_dawn_back.png", // [5] -Z
    //};

    //std::array<std::wstring, 6> skyboxFaces = {
    //   L"assets/skybox/clearOcean/vz_right.png",    // [0] +X
    //   L"assets/skybox/clearOcean/vz_left.png",  // [1] -X
    //   L"assets/skybox/clearOcean/vz_up.png",   // [2] +Y
    //   L"assets/skybox/clearOcean/vz_down.png", // [3] -Y
    //   L"assets/skybox/clearOcean/vz_front.png",  // [4] +Z
    //   L"assets/skybox/clearOcean/vz_back.png", // [5] -Z
    //};

    if (!CreateSkyboxSRV(g_pd3dDevice, skyboxFaces, &g_pSkyboxSRV))
    {
        return E_FAIL;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// 解放処理
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if (g_pImmediateContext) g_pImmediateContext->ClearState();

    if (g_pDepthStencilLessEqual) g_pDepthStencilLessEqual->Release();
    if (g_pRasterizerNone) g_pRasterizerNone->Release();
    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_pSkyboxSRV) g_pSkyboxSRV->Release();
    if (g_pConstantBuffer) g_pConstantBuffer->Release();
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if (g_pVertexBuffer) g_pVertexBuffer->Release();
    if (g_pVertexLayout) g_pVertexLayout->Release();
    if (g_pVertexShader) g_pVertexShader->Release();
    if (g_pPixelShader) g_pPixelShader->Release();
    if (g_pDepthStencil) g_pDepthStencil->Release();
    if (g_pDepthStencilView) g_pDepthStencilView->Release();
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain) g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice) g_pd3dDevice->Release();
}

//--------------------------------------------------------------------------------------
// ウィンドウメッセージ処理 (矢印キーで視点回転)
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_LEFT:  g_cameraYaw -= 0.05f; break;
        case VK_RIGHT: g_cameraYaw += 0.05f; break;
        case VK_UP:    g_cameraPitch += 0.05f; break;
        case VK_DOWN:  g_cameraPitch -= 0.05f; break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// 描画処理
//--------------------------------------------------------------------------------------
void Render()
{
    // 背景クリアカラー
    float ClearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ---- 行列計算設定 ----
    RECT rc;
    GetClientRect(g_hWnd, &rc);
    float width = (float)(rc.right - rc.left);
    float height = (float)(rc.bottom - rc.top);

    // プロジェクション行列
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), width / height, 0.1f, 1000.0f);

    // カメラ回転からビュー行列を生成
    XMVECTOR targetLocal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(g_cameraPitch, g_cameraYaw, 0.0f);
    XMVECTOR targetWorld = XMVector3TransformCoord(targetLocal, matRot);
    XMVECTOR upWorld = XMVector3TransformCoord(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), matRot);

    // 架空のカメラ位置 (テスト用に原点からズラしてみる)
    XMVECTOR eyePos = XMVectorSet(10.0f, 5.0f, -20.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, eyePos + targetWorld, upWorld);

    // 【重要】スカイボックス用のビュー行列：平行移動成分(4行目)をゼロにする
    // これにより、カメラがどこに移動してもスカイボックスは絶対にズームしたり追い抜いたりしなくなります。
    XMMATRIX skyboxView = view;
    skyboxView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    // ワールド行列は単位行列（中心は常に原点）
    XMMATRIX world = XMMatrixIdentity();

    // WVP計算
    ConstantBuffer cb;
    cb.WVP = XMMatrixTranspose(world * skyboxView * projection);
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

    // ---- パイプラインの設定 ----
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pSkyboxSRV);
    g_pImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    // スカイボックス用の独自ステートを適用
    g_pImmediateContext->RSSetState(g_pRasterizerNone);
    g_pImmediateContext->OMSetDepthStencilState(g_pDepthStencilLessEqual, 0);

    // 描画実行 (立方体のインデックス数 = 36)
    g_pImmediateContext->DrawIndexed(36, 0, 0);

    // 画面に表示
    g_pSwapChain->Present(0, 0);
}