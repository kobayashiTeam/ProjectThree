
#include "shader.h"

bool Shader::Create(ID3D11Device* pDevice, const wchar_t* vsFileName, const wchar_t* psFileName,
    const D3D11_INPUT_ELEMENT_DESC* layout,  UINT layoutCount)
{
    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    // 1. 頂点シェーダーのコンパイルと生成
    hr = D3DCompileFromFile(vsFileName, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(hr)) {
        if (pErrorBlob) {
            // 出力ウィンドウにコンパイルエラーの詳細を表示
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        } return false; }

    hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }

    // 2. 頂点レイアウトの作成
    hr = pDevice->CreateInputLayout(layout, layoutCount, pVSBlob->GetBufferPointer(), 
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