// Graphics.cpp
#include "graphics.h"

Graphics::Graphics()
{
}

Graphics::~Graphics()
{
    Finalize();
}

bool Graphics::Initialize(HWND hWnd, int width, int height)
{
    // スワップチェーンの設定
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//ここがディスプレイ出力の限界？
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    //これはなんだろう？
    //GPUに要求する、最低機能レベルのサイン
    //9_1,10_0にしてみたら窓が一瞬出て消えた。
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    // 1. Device, Context, SwapChain の生成
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 1, D3D11_SDK_VERSION, &sd,
        &m_swapChain, &m_device, &featureLevel, &m_context
    );
    if (FAILED(hr)) return false;

    // 2. レンダーターゲットビューの作成
    ID3D11Texture2D* pBackBuffer = nullptr;
    //getBufferが情報取得だけじゃなくて、pBackBufferにいれたのか？
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return false;
    //これがカラーバッファーのことか？ここに最終的に出力されたものが表示されるのか？
    //正しい。インデックス0にPSなどの処理後の出力先にここに送られる。
    hr = m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) return false;

    // 3. 深度バッファの作成
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

    hr = m_device->CreateTexture2D(&descDepth, nullptr, &m_depthStencilBuffer);
    if (FAILED(hr)) return false;

    // 4. 深度ステンシルステートの作成（OpenGLの glEnable(GL_DEPTH_TEST) 相当）
    // 2. ビュー（DSV）の作成（★ここを上に移動）
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    hr = m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &descDSV, &m_depthStencilView);
    if (FAILED(hr)) return false;

    // 3. 深度ステンシルステート（説明書）の作成
    //普通のステート
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = FALSE;

    hr = m_device->CreateDepthStencilState(&dsDesc, &m_defaultDepthStencilState);
    if (FAILED(hr)) return false; // 失敗時の安全弁

    // ==========================================
    // 後半：準備できたモノをまとめてパイプラインに連結（セット）する
    // ==========================================

    // 4. レンダーターゲットと深度バッファ（窓口）をセット
    m_context->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView.Get());

    // 5. 深度テストのルール（説明書）をセット
    m_context->OMSetDepthStencilState(m_defaultDepthStencilState.Get(), 0);

    // 4. ラスタライザーステート（背面カリング）の作成
    D3D11_RASTERIZER_DESC dr = {};
    dr.FillMode = D3D11_FILL_SOLID;
    dr.CullMode = D3D11_CULL_BACK;
    dr.FrontCounterClockwise = FALSE;

    hr = m_device->CreateRasterizerState(&dr, &m_rasterizerState);
    if (FAILED(hr)) return false;

    m_context->RSSetState(m_rasterizerState.Get());

    // 5. ビューポートの設定
    //これもなんだっけ？描画出力先を細かい部分で描画したりするんだっけ？
    //width,heightを1.2にすると出力が左上に限定された。ミニマップなどに使えそう
    D3D11_VIEWPORT vp;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_context->RSSetViewports(1, &vp);

    return true;
}

void Graphics::Finalize()
{
    if (m_rasterizerState) { m_rasterizerState->Release();  m_rasterizerState = nullptr; }
    if (m_depthStencilView) { m_depthStencilView->Release(); m_depthStencilView = nullptr; }
    if (m_depthStencilBuffer) { m_depthStencilBuffer->Release();     m_depthStencilBuffer = nullptr; }
    if (m_renderTargetView) { m_renderTargetView->Release(); m_renderTargetView = nullptr; }
    if (m_swapChain) { m_swapChain->Release();       m_swapChain = nullptr; }
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release();        m_device = nullptr; }
}

void Graphics::BeginScene(float r, float g, float b, float a)
{
    OutputDebugString(L"BeginScene called\n");
    float clearColor[4] = { r, g, b, a };
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Graphics::EndScene()
{
    m_swapChain->Present(1, 0);
}