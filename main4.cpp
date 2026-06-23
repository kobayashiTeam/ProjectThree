// =============================================================
//  InstancingDemo.cpp
//  D3D11 インスタンシング 最小デモ（cpp 1枚）
//
//  将司さんのコードとの対応：
//    ・マルチストリームVB  slot0=Pos, slot1=Normal, slot2=Color, slot3=UV
//    ・インスタンスバッファ slot4（INSTANCE_WORLD0-3, PER_INSTANCE_DATA）
//    ・cbuffer b0=PerFrameBuffer, b2=PerMaterialBuffer（b1はスキップ）
//    ・シェーダは Lit_InstancingShader.hlsl を使用
// =============================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

// =============================================================
// 定数バッファ構造体（将司さんのシェーダのレイアウトと 1:1 対応）
// =============================================================

// b0
struct PerFrameBuffer
{
    XMFLOAT4X4 mView;
    XMFLOAT4X4 mProjection;
    XMFLOAT4   vLightPos;
    XMFLOAT4   vLightColor;
    XMFLOAT4   vEyePos;
    XMFLOAT4   vAttenuation;
};

// b2（b1はスキップ）
struct PerMaterialBuffer
{
    XMFLOAT4 vMaterialColor;
};

// スロット4に送るインスタンスデータ（ワールド行列1枚 = float4 × 4）
struct InstanceData
{
    XMFLOAT4 row0;
    XMFLOAT4 row1;
    XMFLOAT4 row2;
    XMFLOAT4 row3;
};

// =============================================================
// ヘルパー：XMMatrix → InstanceData（行ベクトル順で格納）
//   XMStoreFloat4x4 は行列を row-major で保存するので
//   _11-_14 が row0, _21-_24 が row1 ... となる
// =============================================================
InstanceData MatrixToInstanceData(XMMATRIX m)
{
    XMFLOAT4X4 f;
    XMStoreFloat4x4(&f, m);           // row-major で書き出し
    InstanceData d;
    d.row0 = { f._11, f._12, f._13, f._14 };
    d.row1 = { f._21, f._22, f._23, f._24 };
    d.row2 = { f._31, f._32, f._33, f._34 };
    d.row3 = { f._41, f._42, f._43, f._44 };
    return d;
}

// =============================================================
// グローバル D3D11 オブジェクト
// =============================================================
static HWND                     g_hWnd = nullptr;
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_pRTV = nullptr;
static ID3D11DepthStencilView* g_pDSV = nullptr;

// シェーダ・レイアウト
static ID3D11VertexShader* g_pVS = nullptr;
static ID3D11PixelShader* g_pPS = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;

// 頂点バッファ（マルチストリーム: slot 0-3）
static ID3D11Buffer* g_pVB_Pos = nullptr;
static ID3D11Buffer* g_pVB_Normal = nullptr;
static ID3D11Buffer* g_pVB_Color = nullptr;
static ID3D11Buffer* g_pVB_UV = nullptr;
static ID3D11Buffer* g_pIB = nullptr;
static UINT                     g_indexCount = 0;

// インスタンスバッファ（slot 4）
static ID3D11Buffer* g_pInstanceBuffer = nullptr;
static UINT                     g_maxInstances = 1000;

// 定数バッファ
static ID3D11Buffer* g_pCB_Frame = nullptr;
static ID3D11Buffer* g_pCB_Material = nullptr;

// テクスチャ・サンプラー（1x1 白テクスチャ）
static ID3D11ShaderResourceView* g_pTextureSRV = nullptr;
static ID3D11SamplerState* g_pSampler = nullptr;

// 深度ステンシルステート
static ID3D11DepthStencilState* g_pDSState = nullptr;

// =============================================================
// 立方体の頂点データ（一辺1, 原点中心）
//   各面2三角形 = 36インデックス
//   法線は面ごとに同じ値（フラット法線）
// =============================================================
static const XMFLOAT3 g_positions[] =
{
    // +Z 面
    {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f},
    // -Z 面
    {+0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f},
    // +Y 面
    {-0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f},
    // -Y 面
    {-0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, +0.5f}, {-0.5f, -0.5f, +0.5f},
    // +X 面
    {+0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}, {+0.5f, +0.5f, +0.5f},
    // -X 面
    {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, -0.5f},
};

static const XMFLOAT3 g_normals[] =
{
    {0,0,+1},{0,0,+1},{0,0,+1},{0,0,+1},
    {0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1},
    {0,+1,0},{0,+1,0},{0,+1,0},{0,+1,0},
    {0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},
    {+1,0,0},{+1,0,0},{+1,0,0},{+1,0,0},
    {-1,0,0},{-1,0,0},{-1,0,0},{-1,0,0},
};

static const XMFLOAT4 g_colors[] =
{
    {1,0,0,1},{1,0,0,1},{1,0,0,1},{1,0,0,1}, // 赤
    {0,1,0,1},{0,1,0,1},{0,1,0,1},{0,1,0,1}, // 緑
    {0,0,1,1},{0,0,1,1},{0,0,1,1},{0,0,1,1}, // 青
    {1,1,0,1},{1,1,0,1},{1,1,0,1},{1,1,0,1}, // 黄
    {1,0,1,1},{1,0,1,1},{1,0,1,1},{1,0,1,1}, // マゼンタ
    {0,1,1,1},{0,1,1,1},{0,1,1,1},{0,1,1,1}, // シアン
};

static const XMFLOAT2 g_uvs[] =
{
    {0,1},{1,1},{1,0},{0,0},
    {0,1},{1,1},{1,0},{0,0},
    {0,1},{1,1},{1,0},{0,0},
    {0,1},{1,1},{1,0},{0,0},
    {0,1},{1,1},{1,0},{0,0},
    {0,1},{1,1},{1,0},{0,0},
};

// 1面 = 4頂点 → 2三角形（0,1,2, 0,2,3）× 6面
static const WORD g_indices[] =
{
     0, 1, 2,  0, 2, 3,
     4, 5, 6,  4, 6, 7,
     8, 9,10,  8,10,11,
    12,13,14, 12,14,15,
    16,17,18, 16,18,19,
    20,21,22, 20,22,23,
};

// =============================================================
// D3D11 初期化
// =============================================================
static bool InitD3D(HWND hWnd, int width, int height)
{
    // --- スワップチェーン ---
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, &featureLevel, 1,
        D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pDevice, nullptr, &g_pContext);
    if (FAILED(hr)) return false;

    // --- レンダーターゲット ---
    ID3D11Texture2D* pBackBuf = nullptr;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuf);
    g_pDevice->CreateRenderTargetView(pBackBuf, nullptr, &g_pRTV);
    pBackBuf->Release();

    // --- 深度バッファ ---
    ID3D11Texture2D* pDepthTex = nullptr;
    D3D11_TEXTURE2D_DESC dtd = {};
    dtd.Width = width;
    dtd.Height = height;
    dtd.MipLevels = 1;
    dtd.ArraySize = 1;
    dtd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dtd.SampleDesc.Count = 1;
    dtd.Usage = D3D11_USAGE_DEFAULT;
    dtd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    g_pDevice->CreateTexture2D(&dtd, nullptr, &pDepthTex);
    g_pDevice->CreateDepthStencilView(pDepthTex, nullptr, &g_pDSV);
    pDepthTex->Release();

    // 深度テストを有効に
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    g_pDevice->CreateDepthStencilState(&dsd, &g_pDSState);

    // --- ビューポート ---
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MaxDepth = 1.0f;
    g_pContext->RSSetViewports(1, &vp);

    return true;
}

// =============================================================
// シェーダ・InputLayout の作成
// =============================================================
static bool CreateShaders()
{
    // InputLayout（将司さんの instancedLayout と同じ構造）
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        // slot 0-3: 頂点属性（PER_VERTEX）
        { "POSITION",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "NORMAL",         0, DXGI_FORMAT_R32G32B32_FLOAT,    1,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "COLOR",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        { "TEXCOORD",       0, DXGI_FORMAT_R32G32_FLOAT,       3,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
        // slot 4: インスタンスデータ（PER_INSTANCE, ステップレート=1）
        { "INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 4,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    UINT layoutCount = ARRAYSIZE(layout);

    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    HRESULT hr = D3DCompileFromFile(
        L"Shaders/TestInstancingShader.hlsl",
        nullptr, nullptr, "VS", "vs_5_0",
        D3DCOMPILE_DEBUG, 0,
        &pVSBlob, &pErrorBlob);

    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    hr = g_pDevice->CreateVertexShader(
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
        nullptr, &g_pVS);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }

    hr = g_pDevice->CreateInputLayout(
        layout, layoutCount,
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
        &g_pInputLayout);
    pVSBlob->Release();
    if (FAILED(hr)) return false;

    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompileFromFile(
        L"Shaders/TestInstancingShader.hlsl",
        nullptr, nullptr, "PS", "ps_5_0",
        D3DCOMPILE_DEBUG, 0,
        &pPSBlob, &pErrorBlob);

    if (FAILED(hr))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    hr = g_pDevice->CreatePixelShader(
        pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
        nullptr, &g_pPS);
    pPSBlob->Release();
    return SUCCEEDED(hr);
}

// =============================================================
// ジオメトリバッファの作成
//   マルチストリームVB（slot 0-3）+ IB + インスタンスバッファ（slot 4）
// =============================================================
static bool CreateGeometry()
{
    // -------- ヘルパーラムダ --------
    auto MakeVB = [&](const void* pData, UINT byteWidth, ID3D11Buffer** ppBuf) -> bool
        {
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = byteWidth;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = pData;
            return SUCCEEDED(g_pDevice->CreateBuffer(&bd, &initData, ppBuf));
        };

    UINT vertCount = ARRAYSIZE(g_positions);

    if (!MakeVB(g_positions, sizeof(XMFLOAT3) * vertCount, &g_pVB_Pos))    return false;
    if (!MakeVB(g_normals, sizeof(XMFLOAT3) * vertCount, &g_pVB_Normal)) return false;
    if (!MakeVB(g_colors, sizeof(XMFLOAT4) * vertCount, &g_pVB_Color))  return false;
    if (!MakeVB(g_uvs, sizeof(XMFLOAT2) * vertCount, &g_pVB_UV))     return false;

    // インデックスバッファ
    g_indexCount = ARRAYSIZE(g_indices);
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(WORD) * g_indexCount;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = g_indices;
        if (FAILED(g_pDevice->CreateBuffer(&bd, &initData, &g_pIB))) return false;
    }

    // インスタンスバッファ（DYNAMIC: 毎フレーム Map/Unmap で更新）
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(InstanceData) * g_maxInstances;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(g_pDevice->CreateBuffer(&bd, nullptr, &g_pInstanceBuffer))) return false;
    }

    return true;
}

// =============================================================
// 定数バッファ・テクスチャ・サンプラーの作成
// =============================================================
static bool CreateConstantBuffersAndTexture()
{
    // b0 PerFrameBuffer
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(PerFrameBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(g_pDevice->CreateBuffer(&bd, nullptr, &g_pCB_Frame))) return false;
    }
    // b2 PerMaterialBuffer
    {
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(PerMaterialBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(g_pDevice->CreateBuffer(&bd, nullptr, &g_pCB_Material))) return false;
    }

    // 1x1 白テクスチャ（ライティングの色だけ見たいため）
    {
        UINT32 white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = 1;
        td.Height = 1;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &white;
        initData.SysMemPitch = sizeof(UINT32);

        ID3D11Texture2D* pTex = nullptr;
        if (FAILED(g_pDevice->CreateTexture2D(&td, &initData, &pTex))) return false;
        HRESULT hr = g_pDevice->CreateShaderResourceView(pTex, nullptr, &g_pTextureSRV);
        pTex->Release();
        if (FAILED(hr)) return false;
    }

    // サンプラー（POINTフィルタ）
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(g_pDevice->CreateSamplerState(&sd, &g_pSampler))) return false;
    }

    return true;
}

// =============================================================
// インスタンスデータの更新（毎フレーム呼ぶ）
// =============================================================
static UINT UpdateInstanceBuffer(float time)
{
    // 5×5×5 = 125個のキューブを等間隔に並べる
    const int   N = 5;
    const float spacing = 2.5f;

    std::vector<InstanceData> instances;
    instances.reserve(N * N * N);

    for (int z = 0; z < N; ++z)
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
            {
                float px = (x - N / 2) * spacing;
                float py = (y - N / 2) * spacing;
                float pz = (z - N / 2) * spacing;

                // インスタンスごとに位相をずらして回転
                float phase = (x + y * N + z * N * N) * 0.3f;
                XMMATRIX rot = XMMatrixRotationY(time + phase);
                XMMATRIX trans = XMMatrixTranslation(px, py, pz);

                // ★ XMMatrixMultiply は「左から右に適用」
                //   rot * trans = ローカル回転してからワールド平行移動
                XMMATRIX world = XMMatrixMultiply(rot, trans);

                // ★ シェーダへ送る前に転置が必要か？
                //   HLSLのmul(vec,mat)は「行ベクトル × 行列」なので、
                //   XMMatrixを「そのまま行ごとにfloat4x4に書き出す」だけでOK。
                //   （XMStoreFloat4x4 → row0=_11~_14 の順で格納）
                instances.push_back(MatrixToInstanceData(world));
            }

    UINT count = (UINT)instances.size();

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(g_pContext->Map(g_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, instances.data(), sizeof(InstanceData) * count);
        g_pContext->Unmap(g_pInstanceBuffer, 0);
    }

    return count;
}

// =============================================================
// 描画（1フレーム分）
// =============================================================
static void Render(float time, int width, int height)
{
    // ---- 画面クリア ----
    const float clearColor[] = { 0.1f, 0.1f, 0.2f, 1.0f };
    g_pContext->ClearRenderTargetView(g_pRTV, clearColor);
    g_pContext->ClearDepthStencilView(g_pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
    g_pContext->OMSetRenderTargets(1, &g_pRTV, g_pDSV);
    g_pContext->OMSetDepthStencilState(g_pDSState, 0);

    // ---- インスタンスデータを GPU へ転送 ----
    UINT instanceCount = UpdateInstanceBuffer(time);

    // ---- PerFrameBuffer を更新して b0 にバインド ----
    {
        XMVECTOR eyePos = XMVectorSet(0.0f, 5.0f, -20.0f, 1.0f);
        XMVECTOR focusPos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        PerFrameBuffer cb0 = {};
        // ★ UpdateSubresource 用に転置してから渡す
        //   （D3D11の定数バッファは column-major、
        //     XMMatrix は row-major なので転置が必要）
        XMStoreFloat4x4(&cb0.mView, XMMatrixTranspose(XMMatrixLookAtLH(eyePos, focusPos, up)));
        XMStoreFloat4x4(&cb0.mProjection, XMMatrixTranspose(
            XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f),
                (float)width / height, 0.1f, 100.0f)));
        cb0.vLightPos = { 5.0f, 10.0f, -5.0f, 0.0f };
        cb0.vLightColor = { 1.0f,  1.0f,  1.0f, 1.0f };
        cb0.vEyePos = { 0.0f,  5.0f, -20.0f, 1.0f };
        cb0.vAttenuation = { 1.0f,  0.05f, 0.005f, 0.0f };

        g_pContext->UpdateSubresource(g_pCB_Frame, 0, nullptr, &cb0, 0, 0);
        g_pContext->VSSetConstantBuffers(0, 1, &g_pCB_Frame);
        g_pContext->PSSetConstantBuffers(0, 1, &g_pCB_Frame);
    }

    // ---- PerMaterialBuffer を b2 にバインド ----
    {
        PerMaterialBuffer cb2 = {};
        cb2.vMaterialColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        g_pContext->UpdateSubresource(g_pCB_Material, 0, nullptr, &cb2, 0, 0);
        g_pContext->PSSetConstantBuffers(2, 1, &g_pCB_Material);
    }

    // ---- シェーダ・レイアウトをバインド ----
    g_pContext->IASetInputLayout(g_pInputLayout);
    g_pContext->VSSetShader(g_pVS, nullptr, 0);
    g_pContext->PSSetShader(g_pPS, nullptr, 0);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ---- マルチストリームVB をまとめてバインド ----
    //   slot 0-3: 頂点属性, slot 4: インスタンスデータ
    ID3D11Buffer* vbs[5] = {
        g_pVB_Pos,          // slot 0: float3  POSITION
        g_pVB_Normal,       // slot 1: float3  NORMAL
        g_pVB_Color,        // slot 2: float4  COLOR
        g_pVB_UV,           // slot 3: float2  TEXCOORD
        g_pInstanceBuffer,  // slot 4: InstanceData (float4×4)  PER_INSTANCE
    };
    UINT strides[5] = {
        sizeof(XMFLOAT3),    // Pos
        sizeof(XMFLOAT3),    // Normal
        sizeof(XMFLOAT4),    // Color
        sizeof(XMFLOAT2),    // UV
        sizeof(InstanceData),// InstanceData
    };
    UINT offsets[5] = { 0, 0, 0, 0, 0 };

    g_pContext->IASetVertexBuffers(0, 5, vbs, strides, offsets);
    g_pContext->IASetIndexBuffer(g_pIB, DXGI_FORMAT_R16_UINT, 0);

    // ---- テクスチャ・サンプラーをバインド ----
    g_pContext->PSSetShaderResources(0, 1, &g_pTextureSRV);
    g_pContext->PSSetSamplers(0, 1, &g_pSampler);

    // ---- DrawIndexedInstanced ----
    //   第1引数: インデックス数（キューブ36枚）
    //   第2引数: インスタンス数
    //   残り   : オフセット全部0
    g_pContext->DrawIndexedInstanced(g_indexCount, instanceCount, 0, 0, 0);

    g_pSwapChain->Present(1, 0);
}

// =============================================================
// クリーンアップ
// =============================================================
static void Cleanup()
{
    if (g_pContext) g_pContext->ClearState();

    auto SafeRelease = [](IUnknown* p) { if (p) p->Release(); };
    SafeRelease(g_pDSState);
    SafeRelease(g_pSampler);
    SafeRelease(g_pTextureSRV);
    SafeRelease(g_pCB_Material);
    SafeRelease(g_pCB_Frame);
    SafeRelease(g_pInstanceBuffer);
    SafeRelease(g_pIB);
    SafeRelease(g_pVB_UV);
    SafeRelease(g_pVB_Color);
    SafeRelease(g_pVB_Normal);
    SafeRelease(g_pVB_Pos);
    SafeRelease(g_pInputLayout);
    SafeRelease(g_pPS);
    SafeRelease(g_pVS);
    SafeRelease(g_pDSV);
    SafeRelease(g_pRTV);
    SafeRelease(g_pSwapChain);
    SafeRelease(g_pContext);
    SafeRelease(g_pDevice);
}

// =============================================================
// WndProc / WinMain
// =============================================================
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) { DestroyWindow(hWnd); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    const int WIDTH = 1280;
    const int HEIGHT = 720;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"InstancingDemo";
    RegisterClassEx(&wc);

    g_hWnd = CreateWindow(L"InstancingDemo", L"D3D11 Instancing Demo",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        WIDTH, HEIGHT, nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hWnd, nCmdShow);

    if (!InitD3D(g_hWnd, WIDTH, HEIGHT)) { Cleanup(); return -1; }
    if (!CreateShaders()) { Cleanup(); return -1; }
    if (!CreateGeometry()) { Cleanup(); return -1; }
    if (!CreateConstantBuffersAndTexture()) { Cleanup(); return -1; }

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
            Render(time, WIDTH, HEIGHT);
        }
    }

    Cleanup();
    return 0;
}