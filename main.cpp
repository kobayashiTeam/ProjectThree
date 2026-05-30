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
// ★★★ 深度バッファのために追加 ★★★
ID3D11Texture2D* g_pDepthStencil = nullptr;      // 深度バッファの実体（テクスチャ）
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;  // 深度バッファの窓口（DSV）
ID3D11RasterizerState* g_pRasterizerState = nullptr;   // カリング設定用のステート

// ★★★ 三角形描画のために追加するオブジェクト ★★★
ID3D11VertexShader* g_pVertexShader = nullptr;  // 頂点シェーダー
ID3D11PixelShader* g_pPixelShader = nullptr;   // ピクセルシェーダー
ID3D11InputLayout* g_pVertexLayout = nullptr;  // 頂点レイアウト（指示書）
// ★★★ 四角形（インデックス描画）のために追加 ★★★
// 全局変数に追加
ID3D11ShaderResourceView* g_pTextureRV = nullptr; // テクスチャを表示するための窓口（SRV）
ID3D11SamplerState* g_pSamplerLinear = nullptr; // テクスチャの補間設定

//ユーザ定義ファイル
#include "vertex.h"

// ★★★ 定数バッファのために追加 ★★★
// シェーダーに送る定数バッファの構造体（16バイトアライメントに注意）
// ★変更：定数バッファ構造体
// C++側の定数バッファ構造体の変更
struct ConstantBuffer
{
    XMMATRIX mModel;      // 64バイト
    XMMATRIX mView;       // 64バイト
    XMMATRIX mProjection; // 64バイト

    XMFLOAT4 vLightPos;   // ★変更：ライトの「方向」から「ワールド座標」(x, y, z, w=1.0) に変更
    XMFLOAT4 vLightColor; // 16バイト
    XMFLOAT4 vEyePos;     // 16バイト

    // ★追加：点光源の減衰パラメータ (x: Constant, y: Linear, z: Quadratic, w: ダミー)
    XMFLOAT4 vAttenuation;// 16バイト
};

ID3D11Buffer* g_pConstantBuffer = nullptr; // 定数バッファオブジェクト
float g_Time = 0.0f;                       // 時間計測用

// 既存のオブジェクトの下に追加
#include "Camera.h"
Camera* g_pCamera = nullptr;
#include "mesh.h"
Mesh* g_pCubeMesh = nullptr; // 立方体メッシュ

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

    g_pCubeMesh = new Mesh(); // ← InitDevice の前に移動

    // ★★★ DirectX 11の初期化 ★★★
    if (!InitDevice(hWnd))
    {
        CleanupDevice();
        return 0;
    }

    // InitDevice(hWnd) が成功した後に
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

    // ------------------------------------------------------------------------
    // 【新規追加】深度バッファ（Depth/Stencil Buffer）の作成
    // ------------------------------------------------------------------------
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = sd.BufferDesc.Width;   // レンダーターゲットと同じ幅 (800)
    descDepth.Height = sd.BufferDesc.Height; // レンダーターゲットと同じ高さ (600)
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 深度24bit、ステンシル8bit（LearnOpenGLの標準的な設定と同じ）
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL; // ★深度ステンシルとしてバインド

    hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
    if (FAILED(hr)) return false;

    // 深度バッファのビュー（DSV）を作成
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr)) return false;

    // 4. 【変更】パイプラインにレンダーターゲットと「深度バッファ」を両方セットする
    // 第3引数の nullptr だった場所に g_pDepthStencilView を渡します
    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    // ------------------------------------------------------------------------
    // 【新規追加】ラスタライザーステート（背面カリング）の作成
    // ------------------------------------------------------------------------
    D3D11_RASTERIZER_DESC dr = {};
    dr.FillMode = D3D11_FILL_SOLID;   // 塗りつぶしモード（ワイヤーフレームなら D3D11_FILL_WIREFRAME）
    dr.CullMode = D3D11_CULL_BACK;    // ★背面カリング（裏を向いているポリゴンを描画しない）
    dr.FrontCounterClockwise = FALSE; // ★時計回りを表とする（DirectXの左手系の標準）
    // ※もし立方体の面がいくつか消えてしまったら、頂点のインデックスの指定順が逆（反時計回り）になっている可能性があります。
    // その場合は、ここを TRUE（反時計回りを表）にするか、インデックスの定義を直します。

    hr = g_pd3dDevice->CreateRasterizerState(&dr, &g_pRasterizerState);
    if (FAILED(hr)) return false;

    // パイプラインにカリング設定を適用
    g_pImmediateContext->RSSetState(g_pRasterizerState);

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


    // 7. 頂点インプットレイアウト（指示書）の作成
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        // ★追加：NORMALセマンティクス
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        // ★変更：オフセットを調整（XYZ(3) + Normal(3) = float 6つ分をスキップ）
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        // ★変更：オフセットを調整（XYZ(3) + Normal(3) + COLOR(3) = float 9つ分をスキップ）
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 9, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    // レイアウトの作成（頂点シェーダーのバイナリ情報が照合に必要になります）
    hr = g_pd3dDevice->CreateInputLayout(layout, 4, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
    pVSBlob->Release(); // 頂点シェーダーのバイナリもここで解放してOK
    if (FAILED(hr)) return false;


    // ------------------------------------------------------------------------
     // 8. 頂点バッファ（VBO）の作成 (立方体: 24頂点)
     // ------------------------------------------------------------------------
    SimpleVertex vertices[] =
    {
        // フォーマット：{ X, Y, Z }, { NX, NY, NZ }, { R, G, B }, { U, V }
        // 前面 (Z = -0.5) -> すべて手前（Zのマイナス方向）を向いている
        { -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        // 背面 (Z = 0.5) -> すべて奥（Zのプラス方向）を向いている
        {  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        // 上面 (Y = 0.5)  -> { 0.0f,  1.0f,  0.0f }//追記
        {  -0.5f,  0.5f, 0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        // 下面 (Y = -0.5) -> { 0.0f, -1.0f,  0.0f }//いったん赤色に
        { -0.5f, -0.5f, -0.5f,  0.0f,  -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f,  -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f,  -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f,  -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        // 左側面 (X = -0.5) -> {-1.0f,  0.0f,  0.0f }
        { -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
        // 右側面 (X = 0.5)  -> { 1.0f,  0.0f,  0.0f }
        {  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f },
    };


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

    // ------------------------------------------------------------------------
    // 8. 9. 【変更】メッシュクラスを利用してバッファを作成
    // ------------------------------------------------------------------------
    UINT vertexCount = sizeof(vertices) / sizeof(SimpleVertex);
    UINT indexCount = sizeof(indices) / sizeof(DWORD);

    if (!g_pCubeMesh->Create(g_pd3dDevice, vertices, vertexCount, indices, indexCount))
    {
        return false;
    }

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
    g_Time += 0.01f;

    // ★画面クリア（これがないと前フレームの残像が残る）
    float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ★パイプライン設定（毎フレーム必須）
    g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);

    g_pImmediateContext->PSSetShaderResources(0, 1, &g_pTextureRV);
    g_pImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

    // カメラ・プロジェクション
    XMVECTOR eye = XMVectorSet(0.0f, 2.0f, -4.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_pCamera->Update(eye, at, up); // カメラの更新

    
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

    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
    g_pImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer);
	g_pCubeMesh->Render(g_pImmediateContext);

    // --- 2回目：電球キューブ ---
    XMMATRIX mLightModel =
        XMMatrixScaling(0.1f, 0.1f, 0.1f) *
        XMMatrixTranslation(lightX, lightY, lightZ);

    cb.mModel = XMMatrixTranspose(mLightModel);
    cb.vLightColor.w = 0.0f; // ライト計算スキップフラグ

    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
    g_pCubeMesh->Render(g_pImmediateContext);

    g_pSwapChain->Present(1, 0);
}
// ---------------------------------------------------------
// 後片付け関数
// ---------------------------------------------------------
void CleanupDevice()
{
    // 追加したオブジェクトの解放
    if (g_pDepthStencilView)  g_pDepthStencilView->Release(); // ビュー（窓口）を先に
    if (g_pDepthStencil)      g_pDepthStencil->Release();     // 実体（テクスチャ）を後に
    if (g_pRasterizerState)   g_pRasterizerState->Release();
    // 追加したオブジェクトの解放
    if (g_pVertexLayout)    g_pVertexLayout->Release();
    if (g_pPixelShader)     g_pPixelShader->Release();
    if (g_pVertexShader)    g_pVertexShader->Release();
    if(g_pConstantBuffer)g_pConstantBuffer->Release();
    if (g_pSamplerLinear) g_pSamplerLinear->Release();
    if (g_pTextureRV)      g_pTextureRV->Release();
    // 後片付け
    if (g_pCamera) { delete g_pCamera; g_pCamera = nullptr; }
	if (g_pCubeMesh) { delete g_pCubeMesh; g_pCubeMesh = nullptr; }

    // 既存のオブジェクトの解放
    if (g_pRenderTargetView) g_pRenderTargetView->Release();
    if (g_pSwapChain)        g_pSwapChain->Release();
    if (g_pImmediateContext) g_pImmediateContext->Release();
    if (g_pd3dDevice)        g_pd3dDevice->Release();
}