// DeferredLightingPass.h
#include"d3d11.h"
#include"shaderManager.h"
#include"gBufferPass.h"
#include"depthStencilStates.h"
#include"mesh.h"

class DeferredLightingPass {
public:
    bool Initialize(ID3D11Device* device) {
        m_shader = ShaderManager::GetInstance().GetShader(ShaderID::DeferredLighting);

        D3D11_SAMPLER_DESC pointDesc = {};
        pointDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        pointDesc.AddressU = pointDesc.AddressV = pointDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        pointDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        pointDesc.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&pointDesc, m_depthSampler.GetAddressOf())))
            return false;

        return true;
    }

    // GBufferPass・SSAOの結果を受け取って、現在バインド中のMRT（offscreen+bright）に
    // ライティング結果を描き込む。呼び出し前にRTのバインドはRenderer側で済ませておく前提
    void Execute(
        ID3D11DeviceContext* ctx,
        GBufferPass* gbufferPass,
        ID3D11ShaderResourceView* ssaoSRV,
        DepthStencilStates* dsStates,
        Mesh* fullscreenQuad)
    {
        ID3D11ShaderResourceView* gbufferSRVs[5] = {
            gbufferPass->GetAlbedoSRV(),
            gbufferPass->GetNormalSRV(),
            gbufferPass->GetPositionSRV(),
            gbufferPass->GetDepthSRV(),
            ssaoSRV
        };
        ctx->PSSetShaderResources(8, 5, gbufferSRVs);

        m_shader->Bind(ctx);
        ID3D11SamplerState* pointSampler = m_depthSampler.Get();
        ctx->PSSetSamplers(3, 1, &pointSampler);

        dsStates->Bind(ctx, DepthStencilStates::Mode::DepthTest); // SV_Depth書き込みのため必要
        fullscreenQuad->Render(ctx);
    }

private:
    Shader* m_shader = nullptr;
    ComPtr<ID3D11SamplerState> m_depthSampler;
};