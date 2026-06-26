// ShadowCubeMap.cpp
#include "shadowCubeMap.h"
#include "light.h"

void ShadowCubeMap::Initialize(ID3D11Device* device, UINT size)
{
    m_size = size;

    // ArraySize=6 + TEXTURECUBE フラグがTexture2Dとの唯一の違い
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = size;
    texDesc.Height = size;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 6;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // ← スカイボックスと同じ

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr)) return;

    // DSV：6枚まとめて1つのDSVで扱う
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.MipSlice = 0;
    dsvDesc.Texture2DArray.FirstArraySlice = 0;
    dsvDesc.Texture2DArray.ArraySize = 6; // 全6面

    hr = device->CreateDepthStencilView(m_texture.Get(), &dsvDesc, &m_dsv);
    if (FAILED(hr)) return;

    // SRV：TEXTURECUBE として読む
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    hr = device->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
    if (FAILED(hr)) return;
}

void ShadowCubeMap::BeginRender(ID3D11DeviceContext* ctx)
{
    // ShadowMapと全く同じ
    ctx->OMSetRenderTargets(0, nullptr, m_dsv.Get());
    ctx->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width = (float)m_size;
    vp.Height = (float)m_size;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void ShadowCubeMap::EndRender(ID3D11DeviceContext* ctx)
{
    // ShadowMapと全く同じ
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
}

std::array<DirectX::XMMATRIX, 6> ShadowCubeMap::GetLightSpaceMatrices() const
{
    std::array<DirectX::XMMATRIX, 6> matrices;
    DirectX::XMMATRIX proj = m_pLight->GetProjectionMatrix();
    for (int i = 0; i < 6; i++)
        matrices[i] = m_pLight->GetViewMatrix(i) * proj;
    return matrices;
}