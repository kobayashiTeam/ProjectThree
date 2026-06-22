// =============================================================================
// NormalVizSample.cpp
// D3D11 Geometry Shader 法線可視化サンプル（1ファイル完結）
//
// 構成:
//   - Win32ウィンドウ
//   - D3D11デバイス＋スワップチェーン
//   - VS → GS → PS パイプライン
//   - cbuffer: b0=PerFrame(ViewProj), b1=PerObject(World)
//   - 三角形1枚をメッシュとして描画 ＋ GSで法線ラインを生成
//
// 依存: d3d11.lib, d3dcompiler.lib, dxgi.lib, DirectXTK(SimpleMath)
// =============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <directxtk/SimpleMath.h>
#include <wrl/client.h>
#include <stdexcept>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::SimpleMath;

// =============================================================================
// HLSL ソース（インライン埋め込み）
// =============================================================================

// --- 通常メッシュ用 (VS + PS) ---
static const char* g_MeshShaderSrc = R"(
cbuffer PerFrameBuffer : register(b0)
{
    matrix viewProj;
};

cbuffer PerObjectBuffer : register(b1)
{
    matrix world;
};

struct VSInput
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 posH   : SV_POSITION;
    float3 normal : NORMAL;
    float3 color  : COLOR;
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    float4 worldPos = mul(float4(input.pos, 1.0f), world);
    output.posH     = mul(worldPos, viewProj);
    // 法線はワールド変換（今回はスケールなし前提でOK）
    output.normal   = mul(float4(input.normal, 0.0f), world).xyz;
    output.color    = float3(0.8f, 0.8f, 0.8f);
    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    // 簡易ランバート（光源は固定）
    float3 lightDir = normalize(float3(1.0f, 1.0f, -1.0f));
    float  diff     = saturate(dot(normalize(input.normal), lightDir));
    float3 col      = input.color * (0.2f + 0.8f * diff);
    return float4(col, 1.0f);
}
)";

// --- 法線可視化用 (VS + GS + PS) ---
static const char* g_NormalVizShaderSrc = R"(
cbuffer PerFrameBuffer : register(b0)
{
    matrix viewProj;
};

cbuffer PerObjectBuffer : register(b1)
{
    matrix world;
};

// ----------------------------------------
// VS: クリップ空間ではなくワールド空間を渡す
//     （GSでラインを生成してからクリップ変換する）
// ----------------------------------------
struct VSInput
{
    float3 pos    : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float3 worldPos : WORLDPOS;   // ワールド空間の頂点位置
    float3 worldNrm : WORLDNORM;  // ワールド空間の法線
};

VSOutput VS(VSInput input)
{
    VSOutput output;
    output.worldPos = mul(float4(input.pos,    1.0f), world).xyz;
    output.worldNrm = mul(float4(input.normal, 0.0f), world).xyz;
    return output;
}

// ----------------------------------------
// GS: 各頂点から法線方向に線分（2頂点）を生成
// ----------------------------------------
struct GSOutput
{
    float4 posH  : SV_POSITION;
    float3 color : COLOR;
};

[maxvertexcount(6)]  // 三角形の3頂点 × 2頂点(線分) = 6
void GS(triangle VSOutput input[3], inout LineStream<GSOutput> lineStream)
{
    float normalLength = 0.2f;  // 法線ラインの長さ（ワールド単位）

    for (int i = 0; i < 3; i++)
    {
        float3 base = input[i].worldPos;
        float3 tip  = base + normalize(input[i].worldNrm) * normalLength;

        GSOutput v0, v1;

        // 根元（白）
        v0.posH  = mul(float4(base, 1.0f), viewProj);
        v0.color = float3(1.0f, 1.0f, 0.0f); // 黄色

        // 先端（緑）
        v1.posH  = mul(float4(tip, 1.0f), viewProj);
        v1.color = float3(0.0f, 1.0f, 0.0f); // 緑

        lineStream.Append(v0);
        lineStream.Append(v1);
        lineStream.RestartStrip(); // 線分ごとにリスタート
    }
}

// ----------------------------------------
// PS: 色をそのまま出力
// ----------------------------------------
float4 PS(GSOutput input) : SV_TARGET
{
    return float4(input.color, 1.0f);
}
)";

// =============================================================================
// cbuffer 対応 struct
// =============================================================================
struct PerFrameBuffer
{
    Matrix viewProj; // row_major で転置済みを渡す
};

struct PerObjectBuffer
{
    Matrix world;
};

// =============================================================================
// 頂点レイアウト
// =============================================================================
struct Vertex
{
    Vector3 pos;
    Vector3 normal;
};

// =============================================================================
// ヘルパー: シェーダーコンパイル
// =============================================================================
static ComPtr<ID3DBlob> CompileShader(
    const char* src, const char* entry, const char* target)
{
    ComPtr<ID3DBlob> blob, errBlob;
    HRESULT hr = D3DCompile(
        src, strlen(src),
        nullptr, nullptr, nullptr,
        entry, target,
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        blob.GetAddressOf(),
        errBlob.GetAddressOf());

    if (FAILED(hr))
    {
        std::string msg = "Shader compile failed: ";
        if (errBlob)
            msg += (char*)errBlob->GetBufferPointer();
        throw std::runtime_error(msg);
    }
    return blob;
}

// =============================================================================
// グローバル D3D リソース
// =============================================================================
static HWND                     g_hWnd = nullptr;
static ComPtr<IDXGISwapChain>   g_swapChain;
static ComPtr<ID3D11Device>     g_device;
static ComPtr<ID3D11DeviceContext> g_ctx;
static ComPtr<ID3D11RenderTargetView> g_rtv;
static ComPtr<ID3D11DepthStencilView> g_dsv;

// メッシュ用
static ComPtr<ID3D11VertexShader>   g_meshVS;
static ComPtr<ID3D11PixelShader>    g_meshPS;
static ComPtr<ID3D11InputLayout>    g_inputLayout;

// 法線可視化用
static ComPtr<ID3D11VertexShader>   g_nvVS;
static ComPtr<ID3D11GeometryShader> g_nvGS;
static ComPtr<ID3D11PixelShader>    g_nvPS;

// 共通
static ComPtr<ID3D11Buffer> g_vb;
static ComPtr<ID3D11Buffer> g_ib;
static ComPtr<ID3D11Buffer> g_cbPerFrame;
static ComPtr<ID3D11Buffer> g_cbPerObject;

static ComPtr<ID3D11RasterizerState>  g_rsState;
static ComPtr<ID3D11DepthStencilState> g_dsState;

static int g_width = 1280;
static int g_height = 720;

// =============================================================================
// D3D 初期化
// =============================================================================
static void InitD3D()
{
    // --- スワップチェーン ---
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = g_width;
    scd.BufferDesc.Height = g_height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = g_hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, g_swapChain.GetAddressOf(),
        g_device.GetAddressOf(), &featureLevel,
        g_ctx.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("D3D11CreateDeviceAndSwapChain failed");

    // --- RTV ---
    ComPtr<ID3D11Texture2D> backBuf;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuf.GetAddressOf()));
    g_device->CreateRenderTargetView(backBuf.Get(), nullptr, g_rtv.GetAddressOf());

    // --- DSV ---
    D3D11_TEXTURE2D_DESC dsd = {};
    dsd.Width = g_width;
    dsd.Height = g_height;
    dsd.MipLevels = 1;
    dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsd.SampleDesc.Count = 1;
    dsd.Usage = D3D11_USAGE_DEFAULT;
    dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> dsTex;
    g_device->CreateTexture2D(&dsd, nullptr, dsTex.GetAddressOf());
    g_device->CreateDepthStencilView(dsTex.Get(), nullptr, g_dsv.GetAddressOf());

    // --- InputLayout & メッシュシェーダー ---
    {
        auto vsBlob = CompileShader(g_MeshShaderSrc, "VS", "vs_5_0");
        auto psBlob = CompileShader(g_MeshShaderSrc, "PS", "ps_5_0");

        g_device->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            nullptr, g_meshVS.GetAddressOf());
        g_device->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
            nullptr, g_meshPS.GetAddressOf());

        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        g_device->CreateInputLayout(
            layout, 2,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            g_inputLayout.GetAddressOf());
    }

    // --- 法線可視化シェーダー ---
    {
        auto vsBlob = CompileShader(g_NormalVizShaderSrc, "VS", "vs_5_0");
        auto gsBlob = CompileShader(g_NormalVizShaderSrc, "GS", "gs_5_0");
        auto psBlob = CompileShader(g_NormalVizShaderSrc, "PS", "ps_5_0");

        g_device->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            nullptr, g_nvVS.GetAddressOf());
        g_device->CreateGeometryShader(
            gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(),
            nullptr, g_nvGS.GetAddressOf());
        g_device->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
            nullptr, g_nvPS.GetAddressOf());
        // InputLayoutはメッシュと同じ頂点フォーマットなので流用
    }

    // --- 頂点バッファ（三角形1枚、法線は上向き） ---
    Vertex vertices[] =
    {
        { Vector3(0.0f,  1.0f, 0.0f), Vector3(0, 1, 0) },
        { Vector3(1.0f, -1.0f, 0.0f), Vector3(0, 1, 0) },
        { Vector3(-1.0f, -1.0f, 0.0f), Vector3(0, 1, 0) },
    };
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vsd = { vertices };
    g_device->CreateBuffer(&vbd, &vsd, g_vb.GetAddressOf());

    // --- インデックスバッファ ---
    UINT indices[] = { 0, 1, 2 };
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA isd = { indices };
    g_device->CreateBuffer(&ibd, &isd, g_ib.GetAddressOf());

    // --- cbuffer: PerFrame (b0) ---
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(PerFrameBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        g_device->CreateBuffer(&bd, nullptr, g_cbPerFrame.GetAddressOf());
    }

    // --- cbuffer: PerObject (b1) ---
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(PerObjectBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        g_device->CreateBuffer(&bd, nullptr, g_cbPerObject.GetAddressOf());
    }

    // --- RasterizerState ---
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    g_device->CreateRasterizerState(&rd, g_rsState.GetAddressOf());

    // --- DepthStencilState ---
    D3D11_DEPTH_STENCIL_DESC dsd2 = {};
    dsd2.DepthEnable = TRUE;
    dsd2.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd2.DepthFunc = D3D11_COMPARISON_LESS;
    g_device->CreateDepthStencilState(&dsd2, g_dsState.GetAddressOf());
}

// =============================================================================
// 描画
// =============================================================================
static void Render(float time)
{
    // --- cbuffer 更新 ---
    // カメラ: Z=-5 から原点を見る
    Matrix view = Matrix::CreateLookAt(
        Vector3(0, 0, -5), Vector3(0, 0, 0), Vector3(0, 1, 0));
    Matrix proj = Matrix::CreatePerspectiveFieldOfView(
        XM_PIDIV4, (float)g_width / g_height, 0.1f, 100.0f);

    // HLSL は column-major（デフォルト）なので転置して渡す
    PerFrameBuffer perFrame;
    perFrame.viewProj = (view * proj).Transpose();

    D3D11_MAPPED_SUBRESOURCE mapped;
    g_ctx->Map(g_cbPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &perFrame, sizeof(perFrame));
    g_ctx->Unmap(g_cbPerFrame.Get(), 0);

    // ゆっくり回転
    Matrix world = Matrix::CreateRotationY(time * 0.5f);
    PerObjectBuffer perObject;
    perObject.world = world.Transpose();

    g_ctx->Map(g_cbPerObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &perObject, sizeof(perObject));
    g_ctx->Unmap(g_cbPerObject.Get(), 0);

    // --- OM / RS / IA 共通設定 ---
    float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    g_ctx->ClearRenderTargetView(g_rtv.Get(), clearColor);
    g_ctx->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    g_ctx->OMSetRenderTargets(1, g_rtv.GetAddressOf(), g_dsv.Get());
    g_ctx->OMSetDepthStencilState(g_dsState.Get(), 0);
    g_ctx->RSSetState(g_rsState.Get());

    D3D11_VIEWPORT vp = { 0.f, 0.f, (float)g_width, (float)g_height, 0.f, 1.f };
    g_ctx->RSSetViewports(1, &vp);

    UINT stride = sizeof(Vertex), offset = 0;
    g_ctx->IASetVertexBuffers(0, 1, g_vb.GetAddressOf(), &stride, &offset);
    g_ctx->IASetIndexBuffer(g_ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    g_ctx->IASetInputLayout(g_inputLayout.Get());

    // cbuffer を VS/GS の両方にバインド（忘れやすいポイント！）
    ID3D11Buffer* cbs[] = { g_cbPerFrame.Get(), g_cbPerObject.Get() };
    g_ctx->VSSetConstantBuffers(0, 2, cbs);
    g_ctx->GSSetConstantBuffers(0, 2, cbs); // ← GS にも必須

    // ==========================================================
    // Pass 1: 通常メッシュ描画（三角形）
    // ==========================================================
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_ctx->VSSetShader(g_meshVS.Get(), nullptr, 0);
    g_ctx->GSSetShader(nullptr, nullptr, 0); // GSなし！明示的にnullを設定
    g_ctx->PSSetShader(g_meshPS.Get(), nullptr, 0);

    g_ctx->DrawIndexed(3, 0, 0);

    // ==========================================================
    // Pass 2: 法線可視化（GS でラインを生成）
    // ==========================================================
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // ↑ 入力はあくまで「三角形」。LineListにしてはいけない！
    //    GSが三角形を受け取り、LineStreamとして出力する。

    g_ctx->VSSetShader(g_nvVS.Get(), nullptr, 0);
    g_ctx->GSSetShader(g_nvGS.Get(), nullptr, 0);
    g_ctx->PSSetShader(g_nvPS.Get(), nullptr, 0);

    g_ctx->DrawIndexed(3, 0, 0);

    // Pass 2 終了後は必ずGSをnullに戻す
    g_ctx->GSSetShader(nullptr, nullptr, 0);

    g_swapChain->Present(1, 0);
}

// =============================================================================
// Win32
// =============================================================================
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    // ウィンドウ登録
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"NormalVizSample";
    RegisterClassEx(&wc);

    RECT rc = { 0, 0, g_width, g_height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindow(
        L"NormalVizSample", L"Normal Visualizer (GS Sample)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hWnd, SW_SHOW);

    try {
        InitD3D();
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Init Error", MB_OK);
        return -1;
    }

    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

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
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float time = (float)(now.QuadPart - start.QuadPart) / freq.QuadPart;
            Render(time);
        }
    }
    return 0;
}