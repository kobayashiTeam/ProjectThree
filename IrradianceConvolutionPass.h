#pragma once
#include<d3d11.h>
#include<wrl/client.h>
#include<DirectXMath.h>
#include"shaderManager.h"
#include"rasterizerStates.h"
#include"depthStencilStates.h"

// スカイボックスのキューブマップを Diffuse IBL 用に畳み込み（Irradiance Convolution）し、
// 小さな Irradiance キューブマップへ焼き込むパス。
// 環境（スカイボックス）が変わらない前提で、起動時に一度だけ Bake() を呼べばよい
// （毎フレームは何もしない＝軽量なIBLという方針に合わせた設計）
class IrradianceConvolutionPass {
public:
    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    bool Initialize(ID3D11Device* device, UINT faceSize = 32) {
        m_faceSize = faceSize;

        // ==========================================
        // 1. Irradianceキューブマップ本体（6面・小サイズ・HDR）
        // ==========================================
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = faceSize;
        texDesc.Height = faceSize;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 6;
        texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, m_texture.GetAddressOf());
        if (FAILED(hr)) return false;

        // 面ごとに1枚ずつRTVを作る（1面ずつDrawするための単純な構成）
        for (int i = 0; i < 6; i++) {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = texDesc.Format;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = i;
            rtvDesc.Texture2DArray.ArraySize = 1;

            hr = device->CreateRenderTargetView(m_texture.Get(), &rtvDesc, m_rtv[i].GetAddressOf());
            if (FAILED(hr)) return false;
        }

        // ライティングシェーダーから読むときはキューブマップとして1枚のSRVにまとめる
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = 1;

        hr = device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srv.GetAddressOf());
        if (FAILED(hr)) return false;

        // ==========================================
        // 2. 環境マップ（スカイボックス）をサンプリングするためのサンプラー
        // ==========================================
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = device->CreateSamplerState(&samplerDesc, m_sampler.GetAddressOf());
        if (FAILED(hr)) return false;

        // ==========================================
        // 3. 立方体の頂点／インデックス（スカイボックスと全く同じ形状・位置のみでよい）
        // ==========================================
        DirectX::XMFLOAT3 positions[] = {
            {-1.0f,  1.0f, -1.0f},
            { 1.0f,  1.0f, -1.0f},
            { 1.0f, -1.0f, -1.0f},
            {-1.0f, -1.0f, -1.0f},
            {-1.0f,  1.0f,  1.0f},
            { 1.0f,  1.0f,  1.0f},
            { 1.0f, -1.0f,  1.0f},
            {-1.0f, -1.0f,  1.0f},
        };
        WORD indices[] = {
            0, 1, 2,  2, 3, 0,
            4, 5, 1,  1, 0, 4,
            3, 2, 6,  6, 7, 3,
            1, 5, 6,  6, 2, 1,
            4, 0, 3,  3, 7, 4,
            5, 4, 7,  7, 6, 5
        };

        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.ByteWidth = sizeof(positions);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vinit = {};
        vinit.pSysMem = positions;
        hr = device->CreateBuffer(&vbd, &vinit, m_vertexBuffer.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_DEFAULT;
        ibd.ByteWidth = sizeof(indices);
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA iinit = {};
        iinit.pSysMem = indices;
        hr = device->CreateBuffer(&ibd, &iinit, m_indexBuffer.GetAddressOf());
        if (FAILED(hr)) return false;

        // ==========================================
        // 4. 面ごとのView*Projection行列を送る定数バッファ（スロット3、スカイボックスと同じ流儀）
        // ==========================================
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.ByteWidth = sizeof(DirectX::XMMATRIX);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&cbd, nullptr, m_cb.GetAddressOf());
        if (FAILED(hr)) return false;

        m_shader = ShaderManager::GetInstance().GetShader(ShaderID::IrradianceConvolution);
        if (!m_shader) return false;

        return true;
    }

    // 起動時に一度だけ呼ぶ：スカイボックスのSRVを渡して焼き込みを実行する
    void Bake(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* environmentCubeSRV,
        RasterizerStates* rasterStates, DepthStencilStates* dsStates)
    {
        if (!environmentCubeSRV || !m_shader) return;

        using namespace DirectX;

        // PointLight::GetViewMatrix(face)と同じ6方向の並び（+X,-X,+Y,-Y,+Z,-Z）
        XMFLOAT3 dirs[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
        };
        XMFLOAT3 ups[6] = {
            {0, 1, 0}, {0, 1, 0},
            {0, 0,-1}, {0, 0, 1},
            {0, 1, 0}, {0, 1, 0}
        };
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);

        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(m_faceSize);
        vp.Height = static_cast<float>(m_faceSize);
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        // 深度バッファを使わないバッチ処理なので深度テストは無効のままでよい
        dsStates->Bind(ctx, DepthStencilStates::Mode::None);
        rasterStates->Bind(ctx, RasterizerStates::CullMode::None);

        m_shader->Bind(ctx);
        ctx->GSSetShader(nullptr, nullptr, 0); // 前段のGSが残っている可能性を潰す

        UINT stride = sizeof(XMFLOAT3);
        UINT offset = 0;
        ID3D11Buffer* vbPtr = m_vertexBuffer.Get();
        ctx->IASetVertexBuffers(0, 1, &vbPtr, &stride, &offset);
        ctx->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D11ShaderResourceView* srvPtr = environmentCubeSRV;
        ID3D11SamplerState* samplerPtr = m_sampler.Get();
        ctx->PSSetShaderResources(0, 1, &srvPtr);
        ctx->PSSetSamplers(0, 1, &samplerPtr);

        for (int face = 0; face < 6; face++) {
            XMVECTOR pos = XMVectorZero();
            XMVECTOR target = XMLoadFloat3(&dirs[face]);
            XMVECTOR up = XMLoadFloat3(&ups[face]);
            XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
            XMMATRIX viewProj = XMMatrixTranspose(view * proj);

            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(ctx->Map(m_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                memcpy(mapped.pData, &viewProj, sizeof(XMMATRIX));
                ctx->Unmap(m_cb.Get(), 0);
            }
            ID3D11Buffer* cbPtr = m_cb.Get();
            ctx->VSSetConstantBuffers(3, 1, &cbPtr);

            ID3D11RenderTargetView* rtv = m_rtv[face].Get();
            ctx->OMSetRenderTargets(1, &rtv, nullptr);
            ctx->DrawIndexed(36, 0, 0);
        }

        // 後片付け：レンダーターゲット／SRVスロットを空けておく
        ID3D11RenderTargetView* nullRTV = nullptr;
        ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
        ID3D11ShaderResourceView* nullSRV = nullptr;
        ctx->PSSetShaderResources(0, 1, &nullSRV);
    }

    ID3D11ShaderResourceView* GetIrradianceSRV() const { return m_srv.Get(); }

private:
    UINT m_faceSize = 32;

    ComPtr<ID3D11Texture2D> m_texture;
    ComPtr<ID3D11RenderTargetView> m_rtv[6];
    ComPtr<ID3D11ShaderResourceView> m_srv;

    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11Buffer> m_indexBuffer;
    ComPtr<ID3D11Buffer> m_cb;
    ComPtr<ID3D11SamplerState> m_sampler;

    Shader* m_shader = nullptr;
};
