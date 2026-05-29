#include<windows.h>
#include <d3d11.h>
#include <d3dcompiler.h> // ★シェーダーコンパイル用に追加
#include <cmath>
#include <DirectXMath.h> // ★追加：DirectXの数学ライブラリ（GLMの代わりに全般的に使用します）

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib") // ★追加

using namespace DirectX; // XMFLOAT4X4 などを使いやすくするため
// ---------------------------------------------------------
// 全局変数（DirectX 11のオブジェクトたち）
// ---------------------------------------------------------
ID3D11Device * g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

// ★★★ 三角形描画のために追加するオブジェクト ★★★
ID3D11VertexShader* g_pVertexShader = nullptr;  // 頂点シェーダー
ID3D11PixelShader* g_pPixelShader = nullptr;   // ピクセルシェーダー
ID3D11InputLayout* g_pVertexLayout = nullptr;  // 頂点レイアウト（指示書）
ID3D11Buffer* g_pVertexBuffer = nullptr;  // 頂点バッファ
// ★★★ 四角形（インデックス描画）のために追加 ★★★
ID3D11Buffer* g_pIndexBuffer = nullptr; // インデックスバッファ
// 全局変数に追加
ID3D11ShaderResourceView* g_pTextureRV = nullptr; // テクスチャを表示するための窓口（SRV）
ID3D11SamplerState* g_pSamplerLinear = nullptr; // テクスチャの補間設定

// C++側の頂点構造体
// 1. C++側の頂点構造体の変更
struct SimpleVertex
{
    float x, y, z;
    float r, g, b;
    float u, v;    // ★追加：UV座標（テクスチャのどこを指すか）
};

// ★★★ 定数バッファのために追加 ★★★
// シェーダーに送る定数バッファの構造体（16バイトアライメントに注意）
struct ConstantBuffer
{
    XMMATRIX mModel;      // 4x4行列 (64バイト)
    XMMATRIX mView;       // 4x4行列 (64バイト)
    XMMATRIX mProjection; // 4x4行列 (64バイト)
}; // 合計192バイト (16の倍数なのでアライメントはOK)]

ID3D11Buffer* g_pConstantBuffer = nullptr; // 定数バッファオブジェクト
float g_Time = 0.0f;                       // 時間計測用

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
    },
        // ★ 3つ目の属性として「TEXCOORD」を追加
    {
        "TEXCOORD",                  // HLSL側のセマンティクス名
        0,                           // インデックス
        DXGI_FORMAT_R32G32_FLOAT,    // float 2つ分（U, V）
        0,                           // 入力スロット
        sizeof(float) * 6,           // ★オフセット：XYZ(3) + RGB(3) = float 6つ分を飛び越えた位置
        D3D11_INPUT_PER_VERTEX_DATA,
        0
    }

    };

    // レイアウトの作成（頂点シェーダーのバイナリ情報が照合に必要になります）
    hr = g_pd3dDevice->CreateInputLayout(layout, 3, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release(); // 頂点シェーダーのバイナリもここで解放してOK
    if (FAILED(hr)) return false;


    // ------------------------------------------------------------------------
     // 8. 頂点バッファ（VBO）の作成 (立方体: 24頂点)
     // ------------------------------------------------------------------------
    SimpleVertex vertices[] =
    {
        // 前面 (Z = -0.5) -> UVが正しく貼れるように
        { -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
        // 背面 (Z = 0.5)
        {  0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
        // 上面 (Y = 0.5)
        { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
        // 下面 (Y = -0.5)
        { -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
        // 左側面 (X = -0.5)
        { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
        // 右側面 (X = 0.5)
        {  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 1.0f,  0.0f, 1.0f },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * 24; // 24頂点分
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
    if (FAILED(hr)) return false;

    // ------------------------------------------------------------------------
     // 9. インデックスバッファ（EBO）の作成 (6面 × 2個の三角形 × 3頂点 = 36個)
     // ------------------------------------------------------------------------
    DWORD indices[] =
    {
        0, 1, 2,    0, 2, 3,    // 前
        4, 5, 6,    4, 6, 7,    // 後
        8, 9, 10,   8, 10, 11,  // 上
        12, 13, 14, 12, 14, 15, // 下
        16, 17, 18, 16, 18, 19, // 左
        20, 21, 22, 20, 22, 23  // 右
    };

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(DWORD) * 36;      // 36個のインデックス分
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitDataIndex = {};
    InitDataIndex.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer(&ibd, &InitDataIndex, &g_pIndexBuffer);
    if (FAILED(hr)) return false;

    // ------------------------------------------------------------------------
// 10. 【新規追加】定数バッファ（Constant Buffer）の作成
// ------------------------------------------------------------------------
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;             // 毎フレームGPU側で更新する
    cbd.ByteWidth = sizeof(ConstantBuffer);      // 構造体のサイズ（必ず16の倍数）
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // ★定数バッファとして設定
    cbd.CPUAccessFlags = 0;

    hr = g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer); // 初期データは空でOK
    if (FAILED(hr)) return false;

    // ------------------------------------------------------------------------
// 11. 【新規追加】簡易テクスチャ（2x2マス）の作成
// ------------------------------------------------------------------------
// 2x2ピクセルのカラーデータ（RGBA各8bit、1ピクセル4バイト）
// 0xFFFFFFFF = 白（全ビット1）, 0xFF000000 = 黒（アルファだけ1）
    UINT32 pixels[4] = {
        0xFFFFFFFF, 0xFF000000, // [白][黒]
        0xFF000000, 0xFFFFFFFF  // [黒][白] （チェッカーフラッグ模様）
    };

    // A. テクスチャの実体（Texture2D）の設定
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 2;                             // 横幅2ピクセル
    td.Height = 2;                            // 縦幅2ピクセル
    td.MipLevels = 1;                         // ミップマップは使わないので1
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // 1ピクセル4バイトの標準フォーマット
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE; // ★シェーダーから読み込むリソースとして設定

    D3D11_SUBRESOURCE_DATA tInitData = {};
    tInitData.pSysMem = pixels;
    tInitData.SysMemPitch = 2 * sizeof(UINT32); // 横1行のバイト数（2ピクセル分）

    ID3D11Texture2D* pTexture2D = nullptr;
    hr = g_pd3dDevice->CreateTexture2D(&td, &tInitData, &pTexture2D);
    if (FAILED(hr)) return false;

    // B. シェーダーに渡すためのビュー（SRV）を作成
    hr = g_pd3dDevice->CreateShaderResourceView(pTexture2D, nullptr, &g_pTextureRV);
    pTexture2D->Release(); // ビューを作ったので実体ポインタは解放してOK
    if (FAILED(hr)) return false;

    // ------------------------------------------------------------------------
    // 12. 【新規追加】サンプラーステートの作成
    // ------------------------------------------------------------------------
    D3D11_SAMPLER_DESC sampDesc = {};
    //sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // ★線形補間（LearnOpenGLのGL_LINEARに相当）
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    // もしドット絵みたいにクッキリさせたい場合は D3D11_FILTER_MIN_MAG_MIP_POINT (GL_NEAREST) にします
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;   // ★UVが1を超えたらリピート（GL_REPEAT）
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
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

    // 時間を進める
    g_Time += 0.01f;

    // ------------------------------------------------------------------------
    // 各種行列の計算 (LearnOpenGLでのglm::関数群に相当)
    // ------------------------------------------------------------------------
    // 1. Model行列 : Y軸を中心に時間で回転させる
    XMMATRIX mModel = XMMatrixRotationY(g_Time);

    // 2. View行列 : カメラの位置、注視点、上方向を設定
    XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -3.0f, 0.0f);  // カメラ位置
    XMVECTOR At = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);   // ターゲット
    XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);   // 上方向ベクトル
    XMMATRIX mView = XMMatrixLookAtLH(Eye, At, Up);       // LH = 左手系(Left-Hand)用

    // 3. Projection行列 : 遠近感（パース）の設定
    // XMConvertToRadians(45.0f) = 45度をラジアンに変換
    XMMATRIX mProjection = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), 800.0f / 600.0f, 0.01f, 100.0f);

    // ------------------------------------------------------------------------
    // GPUへのデータ転送準備（重要：転置処理）
    // ------------------------------------------------------------------------
    ConstantBuffer cb;
    // C++(行優先) から HLSL(デフォルト列優先) へ渡すために転置(Transpose)する
    cb.mModel = XMMatrixTranspose(mModel);
    cb.mView = XMMatrixTranspose(mView);
    cb.mProjection = XMMatrixTranspose(mProjection);

    // GPU上の定数バッファを更新
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);

    // ------------------------------------------------------------------------
    // パイプラインの設定と描画コマンド発行
    // ------------------------------------------------------------------------
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

    // インデックスの型を DXGI_FORMAT_R32_UINT (DWORD用) に変更してセット
    g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pTextureRV);
    g_pImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    // インデックス数が36個になったので、36を指定
    g_pImmediateContext->DrawIndexed(36, 0, 0);

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
    if (g_pIndexBuffer) g_pIndexBuffer->Release();
    if(g_pConstantBuffer)g_pConstantBuffer->Release();
    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_pTextureRV)      g_pTextureRV->Release();

    // 既存のオブジェクトの解放
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain)        g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice)        g_pd3dDevice->Release();
}