#pragma once
// ShadowSystem.h
#include<d3d11.h>
#include<vector>
#include <wrl/client.h>

#include"shader.h"
#include"light.h"
#include"shadowMap.h"
#include"shadowCubeMap.h"
#include"renderQueue.h"
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;


class ShadowSystem {
public:
    bool Initialize(ID3D11Device* pDevice);

    // ===== Directional =====
    void BeginDirectionalPass(ID3D11DeviceContext* ctx) {
        m_shadowMaps[0].BeginRender(ctx);
        m_shadowShader->Bind(ctx);
    }
    void EndDirectionalPass(ID3D11DeviceContext* ctx) {
        m_shadowMaps[0].EndRender(ctx);
    }

    // ===== Point =====
    void BeginPointPass(ID3D11DeviceContext* ctx) {
        m_shadowCubeMaps[0].BeginRender(ctx);
        m_shadowCubeShader->Bind(ctx);
        ctx->GSSetShader(m_shadowCubeGS, nullptr, 0);
    }
    void EndPointPass(ID3D11DeviceContext* ctx) {
        m_shadowCubeMaps[0].EndRender(ctx);
    }

    // ===== Lightingパスで使うためのバインド =====
    void BindForLighting(ID3D11DeviceContext* ctx) {
        auto* srv = m_shadowMaps[0].GetSRV();
        ctx->PSSetShaderResources(3, 1, &srv);
        ID3D11SamplerState* sampler = m_shadowSampler.Get();
        ctx->PSSetSamplers(1, 1, &sampler);

        srv = m_shadowCubeMaps[0].GetSRV();
        ctx->PSSetShaderResources(4, 1, &srv);
        sampler = m_shadowCubeSampler.Get();
        ctx->PSSetSamplers(2, 1, &sampler);
    }

    void UpdatePointLightConstantBuffer(ID3D11DeviceContext* ctx); 
    void UpdateLightDataConstantBuffer(ID3D11DeviceContext* ctx);
    void SubmitShadowPass(RenderQueue& opaqueQueue) {
        opaqueQueue.SetOverrideVS(m_shadowShader);
    }
    //test
    // Scene4用：DirectionalLightの向きを外部から書き換える（既存要素のin-place更新、
    // 配列の再確保はしない）
    void SetDirectionalLightDirection(DirectX::XMFLOAT3 dir) {
        if (!m_directionalLights.empty()) {
            m_directionalLights[0].direction = dir;
        }
    }

private:
    std::vector<DirectionalLight> m_directionalLights;
    std::vector<PointLight> m_pointLights;
    std::vector<ShadowMap> m_shadowMaps;
    std::vector<ShadowCubeMap> m_shadowCubeMaps;

    Shader* m_shadowShader = nullptr;
    Shader* m_shadowCubeShader = nullptr;
    ID3D11GeometryShader* m_shadowCubeGS = nullptr;

    ComPtr<ID3D11SamplerState> m_shadowSampler;
    ComPtr<ID3D11SamplerState> m_shadowCubeSampler;
    ComPtr<ID3D11Buffer> m_pointLightCB;//shadowCubeMaシェーダ使うcb
	ComPtr<ID3D11Buffer> m_lightCB;//全てで使う
};