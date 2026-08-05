#pragma once
#include<d3d11.h>
#include"renderTarget.h"

class GBufferPass {
public:
    bool Initialize(ID3D11Device* device, UINT width, UINT height) {
        m_position = new RenderTarget();
        if (!m_position->Initialize(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
        m_normal = new RenderTarget();
        if (!m_normal->Initialize(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
        m_albedo = new RenderTarget();
        if (!m_albedo->Initialize(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
        m_depthOnly = new RenderTarget();
        if (!m_depthOnly->InitializeDepthOnly(device, width, height)) return false;

        D3D11_SAMPLER_DESC pointDesc = {};
        pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        pointDesc.AddressU = pointDesc.AddressV = pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        pointDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        pointDesc.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&pointDesc, m_depthSampler.GetAddressOf()))) return false;

        return true;
    }

    ~GBufferPass() {
        delete m_position;
        delete m_normal;
        delete m_albedo;
        delete m_depthOnly;
    }

    // MRTのクリア＋バインドのみ。ステート設定とキュー実行はRenderer側に残す
    void Begin(ID3D11DeviceContext* ctx) {
        m_position->Clear(ctx);
        m_normal->Clear(ctx);
        m_albedo->Clear(ctx);
        m_depthOnly->Clear(ctx);

        RenderTarget* targets[3] = { m_albedo, m_normal, m_position };
        RenderTarget::BindMultiple(ctx, 3, targets, m_depthOnly->GetDSV());
    }

    // ゲッター群（SSAO/Lightingパスから使う）
    ID3D11ShaderResourceView* GetAlbedoSRV()   const { return m_albedo->GetSRV(); }
    ID3D11ShaderResourceView* GetNormalSRV()   const { return m_normal->GetSRV(); }
    ID3D11ShaderResourceView* GetPositionSRV() const { return m_position->GetSRV(); }
    ID3D11ShaderResourceView* GetDepthSRV()    const { return m_depthOnly->GetSRV(); }
    ID3D11SamplerState* GetDepthSampler()      const { return m_depthSampler.Get(); }

private:
    RenderTarget* m_position = nullptr;
    RenderTarget* m_normal = nullptr;
    RenderTarget* m_albedo = nullptr;
    RenderTarget* m_depthOnly = nullptr;
    ComPtr<ID3D11SamplerState> m_depthSampler;
};