#include "material.h"
//#include<WICTextureLoader.h> // もしビルドエラーが出たら後述の対策をします
#include <directxtk/WICTextureLoader.h>


Material::Material() : m_pShader(nullptr), m_pTextureRV(nullptr), m_pSamplerLinear(nullptr) {}
Material::~Material() { Cleanup(); }

bool Material::Initialize(ID3D11Device* pDevice, Shader* pShader, 
    const UINT32* pTexturePixels, UINT txtWidth, UINT txtHeight)
{
    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    // 1. シェーダーポインタを貰うだけ
    m_pShader = pShader;
    if (!m_pShader) return false;

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

	//具体的なテクスチャ内容を渡すための構造体
    //代入の型が気になる。pSysMemってなんでもいいのか？
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
    // シェーダー側をバインドさせる
    if (m_pShader) m_pShader->Bind(pContext);

    // テクスチャとサンプラーをバインド (以前のコードのまま)
    pContext->PSSetShaderResources(0, 1, &m_pTextureRV);
    pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
}

void Material::Cleanup()
{
    // ★ m_pShader はマネージャーが管理・解放するので、ここでは delete しない！
    m_pShader = nullptr;

    if (m_pSamplerLinear) { m_pSamplerLinear->Release(); m_pSamplerLinear = nullptr; }
    if (m_pTextureRV) { m_pTextureRV->Release();     m_pTextureRV = nullptr; }
}

bool Material::InitializeFromFile(ID3D11Device* pDevice, Shader* pShader, const wchar_t* pFileName)
{
    HRESULT hr;

    // 1. シェーダーポインタを貰うだけ
    m_pShader = pShader;
    if (!m_pShader) return false;

    // 2. ★ファイルからテクスチャ（SRV）を直接生成する
    // WICTextureLoaderが、PNGやJPGのデコード、D3D11Texture2Dの作成、SRVの生成まで
    // 一発でやってくれます
    hr = DirectX::CreateWICTextureFromFile(pDevice, pFileName, nullptr, &m_pTextureRV);
    if (FAILED(hr))
    {
        // 読み込みに失敗した場合は、デバッグ出力を出すと原因究明が楽になります
        OutputDebugString(L"Failed to load texture file: ");
        OutputDebugString(pFileName);
        OutputDebugString(L"\n");
        return false;
    }

    // 3. サンプラーの作成（既存のコードと全く同じ）
    D3D11_SAMPLER_DESC sampDesc = {};
    // 💡ファイル画像用なのでPOINTからLINEARに変えると綺麗になります
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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