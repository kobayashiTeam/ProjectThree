
// Shader.cpp
#include "shader.h"

bool Shader::Create(ID3D11Device* pDevice, const wchar_t* vsFileName, const wchar_t* psFileName)
{
    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    // 1. 頂点シェーダーのコンパイルと生成
    hr = D3DCompileFromFile(vsFileName, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }

    // 2. 頂点レイアウトの作成（現状のレイアウトをそのまま移植）
    //インデックスバッファ：meshがもつもの。頂点座標とセット
    //レイアウト：頂点バッファの解釈。頂点情報に含まれる
    //種々の属性をパースする
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float) * 6, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(float) * 10, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = pDevice->CreateInputLayout(layout, 4, pVSBlob->GetBufferPointer(), 
        pVSBlob->GetBufferSize(), &m_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr)) return false;

    // 3. ピクセルシェーダーのコンパイルと生成
    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompileFromFile(psFileName, nullptr, nullptr, "PS", "ps_5_0", 0, 0, 
        &pPSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    hr = pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), 
        pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return false;

    return true;
}

void Shader::Bind(ID3D11DeviceContext* pContext)
{
    pContext->IASetInputLayout(m_pVertexLayout);
    pContext->VSSetShader(m_pVertexShader, nullptr, 0);
    pContext->PSSetShader(m_pPixelShader, nullptr, 0);
}

void Shader::Cleanup()
{
    if (m_pVertexLayout) { m_pVertexLayout->Release(); m_pVertexLayout = nullptr; }
    if (m_pPixelShader) { m_pPixelShader->Release();  m_pPixelShader = nullptr; }
    if (m_pVertexShader) { m_pVertexShader->Release(); m_pVertexShader = nullptr; }
}