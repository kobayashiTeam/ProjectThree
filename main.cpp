#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h> // ★シェーダーコンパイル用に追加

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib") // ★追加
// ---------------------------------------------------------
// 全局変数（DirectX 11のオブジェクトたち）
// ---------------------------------------------------------
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// ★★★ 三角形描画のために追加するオブジェクト ★★★
ID3D11VertexShader* g_pVertexShader = nullptr;  // 頂点シェーダー
ID3D11PixelShader* g_pPixelShader = nullptr;   // ピクセルシェーダー
ID3D11InputLayout* g_pVertexLayout = nullptr;  // 頂点レイアウト（指示書）
ID3D11Buffer* g_pVertexBuffer = nullptr;  // 頂点バッファ

// C++側の頂点構造体
struct SimpleVertex
{
    float x, y, z;
    float r, g, b; // ★色データを追加
};

// 関数の前方宣言
bool InitDevice(HWND hWnd);
void CleanupDevice();
void Render();

// ウィンドウプロシージャ
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

// エントリーポイント
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
        0, CLASS_NAME, L"DirectX 11 - Clear Screen",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr
    );

    if (hWnd == nullptr) return 0;

    ShowWindow(hWnd, nCmdShow);

    // ★★★ DirectX 11の初期化 ★★★
    if (!InitDevice(hWnd))
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
            // ★★★ 毎フレームの描画処理 ★★★
            Render();
        }
    }

    // ★★★ 終了時の後片付け ★★★
    CleanupDevice();

    return (int)msg.wParam;
}

// ---------------------------------------------------------
// DirectX 11の初期化関数
// ---------------------------------------------------------
bool InitDevice(HWND hWnd)
{
    // スワップチェーン（画面の設定）の構造体を設定
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;                                     // バックバッファの数（ダブルバッファリングなら1）
    sd.BufferDesc.Width = 800;                              // 画面の幅
    sd.BufferDesc.Height = 600;                             // 画面の高さ
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;      // ピクセルの色フォーマット（RGBA 各8bit）
    sd.BufferDesc.RefreshRate.Numerator = 60;               // リフレッシュレート（分子: 60）
    sd.BufferDesc.RefreshRate.Denominator = 1;             // リフレッシュレート（分母: 1）＝ 60Hz
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;       // このバッファを描画先として使う
    sd.OutputWindow = hWnd;                                 // 紐付けるWin32のウィンドウハンドル
    sd.SampleDesc.Count = 1;                                // マルチサンプリング（MSAA）のカウント数
    sd.SampleDesc.Quality = 0;                              // クオリティレベル
    sd.Windowed = TRUE;                                     // ウィンドウモードで起動（FALSEならフルスクリーン）

    // 機能レベル（どの世代のGPUまでサポートするか。今回はDX11固定）
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    // 1. Device, Context, SwapChain を同時に生成する巨大な関数
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // ビデオカードのアダプタ（nullptrならメインのグラボ）
        D3D_DRIVER_TYPE_HARDWARE,   // ハードウェア（GPU）を使って処理する
        nullptr,                    // ソフトウェアラスタライザのモジュール（使わない）
        0,                          // 開発用フラグ（D3D11_CREATE_DEVICE_DEBUG にするとVSのデバッグが強くなる）
        featureLevels, 1,           // 使用する機能レベルの配列と数
        D3D11_SDK_VERSION,          // おまじない（SDKバージョン）
        &sd,                        // 上で設定したスワップチェーンの設定書
        &g_pSwapChain,              // [出力] 生成されたSwapChainの格納先
        &g_pd3dDevice,              // [出力] 生成されたDeviceの格納先
        &featureLevel,              // [出力] 実際に選ばれた機能レベル
        &g_pImmediateContext       // [出力] 生成されたContextの格納先
    );

    if (FAILED(hr)) return false; // 生成失敗

    // 2. スワップチェーンから「バックバッファ（実体テクスチャ）」を取り出す
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(hr)) return false;

    // 3. バックバッファを指す「描画ターゲットビュー（整理券）」を作る
    hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release(); // 実体テクスチャ自体は、ビューを作った後はもう参照を外してOK
    if (FAILED(hr)) return false;

    // 4. パイプラインに「ここに描画してね」とターゲットを設定する
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    // 5. ビューポート（画面のどこに描画するか）の設定（LearnOpenGLの glViewport に相当）
    D3D11_VIEWPORT vp;
    vp.Width = 800.0f;
    vp.Height = 600.0f;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    // ------------------------------------------------------------------------
    // 6. HLSLシェーダーの読み込みとコンパイル
    // ------------------------------------------------------------------------
    ID3DBlob* pVSBlob = nullptr; // コンパイルされた頂点シェーダーバイナリの一時格納先
    ID3DBlob* pErrorBlob = nullptr;

    // 頂点シェーダーのコンパイル
    hr = D3DCompileFromFile(
        L"Shader.hlsl",             // シェーダーファイル名
        nullptr, nullptr,
        "VS",                       // ★HLSL内の頂点シェーダー関数名
        "vs_5_0",                   // シェーダーモデル（DX11は 5_0）
        0, 0, &pVSBlob, &pErrorBlob
    );
    if (FAILED(hr))
    {
        if (pErrorBlob) pErrorBlob->Release();
        return false;
    }

    // 頂点シェーダーオブジェクトの生成
    hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr)) return false;

    // ピクセルシェーダーのコンパイル
    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompileFromFile(
        L"Shader.hlsl",
        nullptr, nullptr,
        "PS",                       // ★HLSL内のピクセルシェーダー関数名
        "ps_5_0",
        0, 0, &pPSBlob, &pErrorBlob
    );
    if (FAILED(hr))
    {
        if (pErrorBlob) pErrorBlob->Release();
        return false;
    }

    // ピクセルシェーダーオブジェクトの生成
    hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    pPSBlob->Release(); // ピクセルシェーダーはバイナリをもう使わないので解放
    if (FAILED(hr)) return false;


    // ------------------------------------------------------------------------
    // 7. 頂点インプットレイアウト（指示書）の作成
    // ------------------------------------------------------------------------
    // LearnOpenGLの glVertexAttribPointer に相当
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        {
            "POSITION",             // ★HLSLのセマンティクス名と完全一致させる
            0,                      // セマンティクスのインデックス（0番目）
            DXGI_FORMAT_R32G32B32_FLOAT, // データの型（float3 つまり X, Y, Z）
            0,                      // 入力スロット（基本は0）
            0,                      // C++構造体の先頭からのオフセット（0バイト目）
            D3D11_INPUT_PER_VERTEX_DATA, // 頂点データごとに読み込む
            0
        },

        // ★ 2つ目の属性として「COLOR」を追加
    {
        "COLOR",                     // HLSL側のセマンティクス名
        0,                           // インデックス
        DXGI_FORMAT_R32G32B32_FLOAT, // float 3つ分（R, G, B）
        0,                           // 入力スロット（頂点バッファと同じ0番）
        sizeof(float) * 3,           // ★オフセット：最初のXYZ（float×3）を飛び越えた位置からスタート
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }

    };

    // レイアウトの作成（頂点シェーダーのバイナリ情報が照合に必要になります）
    hr = g_pd3dDevice->CreateInputLayout(layout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release(); // 頂点シェーダーのバイナリもここで解放してOK
    if (FAILED(hr)) return false;


    // ------------------------------------------------------------------------
    // 8. 頂点バッファ（VBO）の作成
    // ------------------------------------------------------------------------
    // 三角形の頂点データ（クリップ空間。中心が 0.0、画面端が -1.0 〜 1.0）
    SimpleVertex vertices[] =
    {
        // { 座標(X,Y,Z), 色(R,G,B) }
        { -0.5f,  0.5f, 0.5f,  1.0f, 0.0f, 0.0f }, // 0: 左上（赤）
        {  0.5f,  0.5f, 0.5f,  0.0f, 1.0f, 0.0f }, // 1: 右上（緑）
        {  0.5f, -0.5f, 0.5f,  0.0f, 0.0f, 1.0f }, // 2: 右下（青）
        { -0.5f, -0.5f, 0.5f,  1.0f, 1.0f, 0.0f }  // 3: 左下（黄）
    };
    // ※ bd.ByteWidth = sizeof(SimpleVertex) * 4; などの記述はそのまま使えます！

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;                 // GPUによる読み書きの標準的な設定
    bd.ByteWidth = sizeof(SimpleVertex) * 3;        // バッファ全体のサイズ（3頂点分）
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // 「頂点バッファ」としてバインドする設定
    bd.CPUAccessFlags = 0;                          // CPUからは直接アクセスしない

    // サブソース（初期化用のデータの実体）の設定
    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;                    // C++側の配列の先頭ポインタ

    // バッファの生成
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr)) return false;

    return true;
}

// ---------------------------------------------------------
// 毎フレームの描画関数
// ---------------------------------------------------------
void Render()
{
    float clearColor[] = { 0.392f, 0.584f, 0.929f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    // ★★★ ここから描画処理を追加 ★★★

    // 1. パイプラインに「頂点レイアウト」を設定
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    // 2. パイプラインに「頂点バッファ」を設定（VAOがないので毎回指定する）
    UINT stride = sizeof(SimpleVertex); // 頂点1個分のバイトサイズ
    UINT offset = 0;                    // バッファのどこから読み始めるか
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // 3. プリミティブ・トポロジー（どういうトポロジーで描くか。今回は三角形リスト）の設定
    // LearnOpenGLの GL_TRIANGLES に相当
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 4. 使用する「シェーダー」を設定
    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

    // 5. 描画！（3つの頂点を使って描画する。引数は「頂点数」と「開始インデックス」）
    // LearnOpenGLの glDrawArrays(GL_TRIANGLES, 0, 3) に相当
    g_pImmediateContext->Draw(3, 0);

    // ★★★ ここまで ★★★

    g_pSwapChain->Present(1, 0);
}

// ---------------------------------------------------------
// 後片付け関数
// ---------------------------------------------------------
void CleanupDevice()
{
    // 追加したオブジェクトの解放
    if (g_pVertexBuffer)    g_pVertexBuffer->Release();
    if (g_pVertexLayout)    g_pVertexLayout->Release();
    if (g_pPixelShader)     g_pPixelShader->Release();
    if (g_pVertexShader)    g_pVertexShader->Release();

    // 既存のオブジェクトの解放
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain)        g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice)        g_pd3dDevice->Release();
}