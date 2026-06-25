#include"shadowMap.h"
#include"light.h"

void ShadowMap::Initialize(ID3D11Device* pDevice, UINT size)
{
    m_size = size;

    // R32_TYPELESSで作る（DSVとSRVで型を使い分けるため）
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = size;
    texDesc.Height = size;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS; // ← ポイント
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = pDevice->CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr)) return;

    // DSV（深度書き込み用）
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 書き込みはD32
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = pDevice->CreateDepthStencilView(m_texture.Get(), &dsvDesc, &m_dsv);
    if (FAILED(hr)) return;

    // SRV（シェーダー読み取り用）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // 読み取りはR32
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    hr = pDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
    if (FAILED(hr)) return;
}


DirectX::XMMATRIX ShadowMap::GetLightSpaceMatrix() const
{
    // 呼び出されるたびにライトの現在状態から計算する
    DirectX::XMMATRIX view = m_pLight->GetViewMatrix();
    DirectX::XMMATRIX proj = m_pLight->GetProjectionMatrix();
    return view * proj;
}


void ShadowMap::BeginRender(ID3D11DeviceContext* ctx) {

}