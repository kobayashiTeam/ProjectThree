#pragma once
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
        ctx->GSSetShader(nullptr, nullptr, 0);
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
    // DirectionalLightの方向を更新する
    void SetDirectionalLightDirection(DirectX::XMFLOAT3 dir) {
        if (!m_directionalLights.empty()) {
            m_directionalLights[0].direction = dir;
        }
    }
    // PointLightの位置を更新する
    void SetPointLightPosition(DirectX::XMFLOAT3 pos) {
        if (!m_pointLights.empty()) {
            m_pointLights[0].position = pos;
        }
    }

    // 有効化するライト種別を設定する
    void SetLightVisibilityMode(LightVisibilityMode mode) {
        m_lightVisibilityMode = mode;
    }

private:
    std::vector<DirectionalLight> m_directionalLights;
    std::vector<PointLight> m_pointLights;
    LightVisibilityMode m_lightVisibilityMode = LightVisibilityMode::Both;
    std::vector<ShadowMap> m_shadowMaps;
    std::vector<ShadowCubeMap> m_shadowCubeMaps;

    Shader* m_shadowShader = nullptr;
    Shader* m_shadowCubeShader = nullptr;
    ID3D11GeometryShader* m_shadowCubeGS = nullptr;

    ComPtr<ID3D11SamplerState> m_shadowSampler;
    ComPtr<ID3D11SamplerState> m_shadowCubeSampler;
    // ShadowCubeMap用シェーダーの定数バッファ
    ComPtr<ID3D11Buffer> m_pointLightCB;
	ComPtr<ID3D11Buffer> m_lightCB;
};