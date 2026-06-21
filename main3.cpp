// ============================================================
// main.cpp  ―  D3D11 PointCloud 最小サンプル（修正版）
// ============================================================
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <cstdlib>
#include <cassert>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ============================================================
// HLSL
// ============================================================
static const char* g_shaderSrc = R"(
cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;        // 64 bytes
    matrix mProjection;  // 64 bytes
    float4 vEyePos;      // 16 bytes  → 合計144bytes、16の倍数OK
};

struct VS_INPUT
{
    float3 Position : POSITION;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
};

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 worldPos = float4(input.Position, 1.0f);
    float4 viewPos  = mul(mView,       worldPos);
    output.Position = mul(mProjection, viewPos);
    return output;
}

float4 PS(VS_OUTPUT input) : SV_Target
{
    return float4(1.0f, 0.65f, 0.2f, 1.0f);
}
)";

// ============================================================
// cbuffer 対応構造体
// ★ XMMATRIX は 16バイトアライン済み、合計144バイト → OK
// ============================================================
struct alignas(16) PerFrameCB
{
    XMMATRIX mView;        // 64
    XMMATRIX mProjection;  // 64
    XMFLOAT4 vEyePos;      // 16
};
static_assert(sizeof(PerFrameCB) % 16 == 0, "cbuffer size must be multiple of 16");

// ============================================================
// グローバル
// ============================================================
static HWND                           g_hWnd = nullptr;
static ComPtr<ID3D11Device>           g_device;
static ComPtr<ID3D11DeviceContext>    g_context;
static ComPtr<IDXGISwapChain>         g_swapChain;
static ComPtr<ID3D11RenderTargetView> g_rtv;
static ComPtr<ID3D11DepthStencilView> g_dsv;
static ComPtr<ID3D11VertexShader>     g_vs;
static ComPtr<ID3D11PixelShader>      g_ps;
static ComPtr<ID3D11InputLayout>      g_inputLayout;
static ComPtr<ID3D11Buffer>           g_vertexBuffer;
static ComPtr<ID3D11Buffer>           g_cbuffer;
static UINT g_pointCount = 0;

// ============================================================
// エラーチェック付きHRESULTマクロ
// ============================================================
static void CheckHR(HRESULT hr, const char* msg)
{
    if (SUCCEEDED(hr)) return;
    char buf[256];
    sprintf_s(buf, "FAILED(0x%08X): %s\n", (unsigned)hr, msg);
    OutputDebugStringA(buf);
    MessageBoxA(nullptr, buf, "D3D Error", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

// ============================================================
// 頂点バッファ作成
// ============================================================
static ComPtr<ID3D11Buffer> CreateVertexBuffer(const void* pData, UINT byteWidth)
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = byteWidth;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = pData;

    ComPtr<ID3D11Buffer> buf;
    CheckHR(g_device->CreateBuffer(&bd, &init, buf.GetAddressOf()),
        "CreateVertexBuffer");
    return buf;
}

// ============================================================
// PointCloud 初期化
// ★ 修正ポイント：カメラZ=0、点群Z=1〜6の前方に配置
// ============================================================
static void InitPointCloud(UINT count, float spread)
{
    std::vector<XMFLOAT3> positions(count);
    for (auto& p : positions)
    {
        p.x = (rand() / (float)RAND_MAX - 0.5f) * spread * 2.0f;
        p.y = (rand() / (float)RAND_MAX - 0.5f) * spread * 2.0f;
        // LH座標：カメラはZ=0で+Z方向を向く → 点はZ>0に置く
        p.z = (rand() / (float)RAND_MAX) * spread + 1.0f;  // Z: 1.0〜3.0
    }
    g_pointCount = count;
    g_vertexBuffer = CreateVertexBuffer(positions.data(), count * sizeof(XMFLOAT3));

    char buf[64];
    sprintf_s(buf, "InitPointCloud: %u points\n", count);
    OutputDebugStringA(buf);
}

// ============================================================
// DrawPointCloud
// ============================================================
static void DrawPointCloud()
{
    if (g_pointCount == 0) return;

    UINT stride = sizeof(XMFLOAT3);
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, g_vertexBuffer.GetAddressOf(), &stride, &offset);
    g_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);  // ★必須
    g_context->Draw(g_pointCount, 0);
}

// ============================================================
// D3D11 初期化
// ============================================================
static void InitD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = { 60, 1 };
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL fl;
    CheckHR(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, g_swapChain.GetAddressOf(),
        g_device.GetAddressOf(), &fl,
        g_context.GetAddressOf()), "CreateDeviceAndSwapChain");

    // RTV
    ComPtr<ID3D11Texture2D> backBuf;
    CheckHR(g_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuf.GetAddressOf())), "GetBuffer");
    CheckHR(g_device->CreateRenderTargetView(backBuf.Get(), nullptr, g_rtv.GetAddressOf()),
        "CreateRTV");

    // Depth Stencil
    D3D11_TEXTURE2D_DESC dsd = {};
    dsd.Width = 800;
    dsd.Height = 600;
    dsd.MipLevels = 1;
    dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count = 1;
    dsd.Usage = D3D11_USAGE_DEFAULT;
    dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depthTex;
    CheckHR(g_device->CreateTexture2D(&dsd, nullptr, depthTex.GetAddressOf()), "CreateDepth");
    CheckHR(g_device->CreateDepthStencilView(depthTex.Get(), nullptr, g_dsv.GetAddressOf()),
        "CreateDSV");

    // Viewport
    D3D11_VIEWPORT vp = { 0.f, 0.f, 800.f, 600.f, 0.f, 1.f };
    g_context->RSSetViewports(1, &vp);

    OutputDebugStringA("InitD3D OK\n");
}

// ============================================================
// シェーダー初期化
// ============================================================
static void InitShaders()
{
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    HRESULT hr = D3DCompile(g_shaderSrc, strlen(g_shaderSrc),
        "PointCloud.hlsl", nullptr, nullptr, "VS", "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        vsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA((char*)errBlob->GetBufferPointer());
        CheckHR(hr, "VS compile");
    }

    hr = D3DCompile(g_shaderSrc, strlen(g_shaderSrc),
        "PointCloud.hlsl", nullptr, nullptr, "PS", "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        psBlob.GetAddressOf(), errBlob.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA((char*)errBlob->GetBufferPointer());
        CheckHR(hr, "PS compile");
    }

    CheckHR(g_device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, g_vs.GetAddressOf()), "CreateVS");
    CheckHR(g_device->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, g_ps.GetAddressOf()), "CreatePS");

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    CheckHR(g_device->CreateInputLayout(layout, 1,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        g_inputLayout.GetAddressOf()), "CreateInputLayout");

    OutputDebugStringA("InitShaders OK\n");
}

// ============================================================
// cbuffer 初期化
// ============================================================
static void InitCBuffer()
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(PerFrameCB);  // 144バイト
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CheckHR(g_device->CreateBuffer(&bd, nullptr, g_cbuffer.GetAddressOf()), "CreateCBuffer");

    char buf[64];
    sprintf_s(buf, "PerFrameCB size = %zu bytes\n", sizeof(PerFrameCB));
    OutputDebugStringA(buf);
}

// ============================================================
// cbuffer 更新
// ★ 修正ポイント：カメラ位置と点群の位置関係を合わせた
//   カメラ : eye=(0,0,0), target=(0,0,1) → +Z方向を見る
//   点群   : Z = 1.0〜3.0
// ============================================================
static void UpdateCBuffer()
{
    XMVECTOR eye = XMVectorSet(0.f, 0.f, 0.f, 1.f);  // 原点
    XMVECTOR target = XMVectorSet(0.f, 0.f, 1.f, 1.f);  // +Z方向
    XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    PerFrameCB cb;
    cb.mView = XMMatrixTranspose(XMMatrixLookAtLH(eye, target, up));
    cb.mProjection = XMMatrixTranspose(
        XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), 800.f / 600.f, 0.1f, 100.f));
    cb.vEyePos = XMFLOAT4(0.f, 0.f, 0.f, 1.f);

    D3D11_MAPPED_SUBRESOURCE ms;
    CheckHR(g_context->Map(g_cbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms), "Map CBuffer");
    memcpy(ms.pData, &cb, sizeof(cb));
    g_context->Unmap(g_cbuffer.Get(), 0);
}

// ============================================================
// 毎フレーム描画
// ============================================================
static void Render()
{
    float clearColor[] = { 0.2f, 0.2f, 0.25f, 1.0f };
    g_context->ClearRenderTargetView(g_rtv.Get(), clearColor);
    g_context->ClearDepthStencilView(g_dsv.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), g_dsv.Get());

    UpdateCBuffer();
    g_context->VSSetConstantBuffers(0, 1, g_cbuffer.GetAddressOf());
    g_context->VSSetShader(g_vs.Get(), nullptr, 0);
    g_context->PSSetShader(g_ps.Get(), nullptr, 0);
    g_context->IASetInputLayout(g_inputLayout.Get());

    DrawPointCloud();

    g_swapChain->Present(1, 0);
}

// ============================================================
// WndProc / WinMain
// ============================================================
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASSEX wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PointCloudWnd";
    RegisterClassEx(&wc);

    g_hWnd = CreateWindowEx(0, L"PointCloudWnd", L"D3D11 PointCloud - ESC to exit",
        WS_OVERLAPPEDWINDOW, 100, 100, 820, 640,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    InitD3D(g_hWnd);
    InitShaders();
    InitCBuffer();
    InitPointCloud(20000, 3.0f);   // 点をめっちゃ増やす

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Render();
        }
    }
    return 0;
}