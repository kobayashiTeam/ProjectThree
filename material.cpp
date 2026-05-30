#include "material.h"

Material::Material()
    : m_pVertexShader(nullptr),
    m_pPixelShader(nullptr),
    m_pVertexLayout(nullptr),
    m_pTextureRV(nullptr),
    m_pSamplerLinear(nullptr)
{
}

Material::~Material()
{
    Cleanup();
}

bool Material::Initialize(ID3D11Device* pDevice, const wchar_t* vsFileName, const wchar_t* psFileName, const UINT32* pTexturePixels, UINT txtWidth, UINT txtHeight)
{
    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    // 1. 頂点シェーダーのコンパイルと生成
    hr = D3DCompileFromFile(vsFileName, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }

    // 2. 頂点レイアウトの作成
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 9, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = pDevice->CreateInputLayout(layout, 4, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pVertexLayout);
    pVSBlob->Release(); // レイアウトを作ったらBlobは不要
    if (FAILED(hr)) return false;

    // 3. ピクセルシェーダーのコンパイルと生成
    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompileFromFile(psFileName, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &pPSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    hr = pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return false;

    // 4. テクスチャの作成
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = txtWidth;
    td.Height = txtHeight;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA tInitData = {};
    tInitData.pSysMem = pTexturePixels;
    tInitData.SysMemPitch = txtWidth * sizeof(UINT32);

    ID3D11Texture2D* pTexture2D = nullptr;
    hr = pDevice->CreateTexture2D(&td, &tInitData, &pTexture2D);
    if (FAILED(hr)) return false;

    hr = pDevice->CreateShaderResourceView(pTexture2D, nullptr, &m_pTextureRV);
    pTexture2D->Release(); // SRVを作ったら本体はリリースしてOK
    if (FAILED(hr)) return false;

    // 5. サンプラーの作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
    if (FAILED(hr)) return false;

    return true;
}

void Material::Bind(ID3D11DeviceContext* pContext)
{
    // パイプラインへの状態セットをこのメソッド内で完結させる
    pContext->IASetInputLayout(m_pVertexLayout);
    pContext->VSSetShader(m_pVertexShader, nullptr, 0);
    pContext->PSSetShader(m_pPixelShader, nullptr, 0);
    pContext->PSSetShaderResources(0, 1, &m_pTextureRV);
    pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
}

void Material::Cleanup()
{
    if (m_pVertexLayout) { m_pVertexLayout->Release();   m_pVertexLayout = nullptr; }
    if (m_pPixelShader) { m_pPixelShader->Release();    m_pPixelShader = nullptr; }
    if (m_pVertexShader) { m_pVertexShader->Release();   m_pVertexShader = nullptr; }
    if (m_pSamplerLinear) { m_pSamplerLinear->Release();  m_pSamplerLinear = nullptr; }
    if (m_pTextureRV) { m_pTextureRV->Release();      m_pTextureRV = nullptr; }
}