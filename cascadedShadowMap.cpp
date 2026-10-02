#include "cascadedShadowMap.h"

bool CascadedShadowMap::Initialize(ID3D11Device* pDevice, UINT size)
{
    m_size = size;

    // ShadowMapと同じくR32_TYPELESSで作り、DSVとSRVで型を使い分ける
    // 違いはArraySizeだけ（1枚→NUM_CASCADES枚）
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = size;
    texDesc.Height = size;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = NUM_CASCADES;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = pDevice->CreateTexture2D(&texDesc, nullptr, &m_texture);
    if (FAILED(hr)) return false;

    // DSV：段ごとに「配列のi番目の1枚だけ」を指すビューを作る
    for (int i = 0; i < NUM_CASCADES; i++)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;

        hr = pDevice->CreateDepthStencilView(m_texture.Get(), &dsvDesc, &m_dsv[i]);
        if (FAILED(hr)) return false;
    }

    // SRV：配列全体を1つのビューで見る（シェーダー側はTexture2DArrayで受ける）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = NUM_CASCADES;

    hr = pDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, &m_srv);
    if (FAILED(hr)) return false;

    return true;
}

void CascadedShadowMap::BeginRender(ID3D11DeviceContext* ctx, int cascadeIndex)
{
    // 前フレームのライティングでt15にSRVとして刺さったままだと、
    // 同じテクスチャをDSVにした瞬間にランタイムが強制的に外して警告を出すため、先に明示的に外しておく
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(15, 1, &nullSRV);

    ctx->OMSetRenderTargets(0, nullptr, m_dsv[cascadeIndex].Get());
    ctx->ClearDepthStencilView(m_dsv[cascadeIndex].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width = (float)m_size;
    vp.Height = (float)m_size;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void CascadedShadowMap::EndRender(ID3D11DeviceContext* ctx)
{
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
}
