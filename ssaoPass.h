#include<d3d11.h>
#include"renderTarget.h"
#include"shaderManager.h"
#include"mesh.h"
#include"graphicsCommon.h"


class SSAOPass {
public:
    bool Initialize(ID3D11Device* device, UINT width, UINT height) {
        m_rawRT = new RenderTarget();
        if (!m_rawRT->Initialize(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
        m_blurRT = new RenderTarget();
        if (!m_blurRT->Initialize(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;

        if (!initNoiseTexture(device)) return false;
        if (!initSamplers(device)) return false;
        if (!initConstantBuffer(device, (float)width, (float)height)) return false;

        m_shader = ShaderManager::GetInstance().GetShader(ShaderID::SSAO);
        m_blurShader = ShaderManager::GetInstance().GetShader(ShaderID::SSAOBlur);
        return true;
    }

    ~SSAOPass() {
        delete m_rawRT;
        delete m_blurRT;
    }

    // GBufferPassから直接SRVを受け取る形にすることで依存関係を引数で明示する
    ID3D11ShaderResourceView* Execute(ID3D11DeviceContext* ctx,
        ID3D11ShaderResourceView* gbufferNormalSRV,
        ID3D11ShaderResourceView* gbufferPositionSRV,
        Mesh* fullscreenQuad)
    {
        // 1. SSAO生成
        m_rawRT->Clear(ctx);
        m_rawRT->Bind(ctx);

        ID3D11ShaderResourceView* inputs[3] = {
            gbufferNormalSRV, gbufferPositionSRV, m_noiseTextureSRV.Get()
        };
        ctx->PSSetShaderResources(0, 3, inputs);

        ID3D11SamplerState* samplers[] = { m_samPointClamp.Get(), m_samPointWrap.Get() };
        ctx->PSSetSamplers(4, 2, samplers);

        updateConstantBuffer(ctx);
        m_shader->Bind(ctx);
        fullscreenQuad->Render(ctx);

        // 2. ブラー
        m_blurRT->Clear(ctx);
        m_blurRT->Bind(ctx);
        auto* rawSRV = m_rawRT->GetSRV();
        ctx->PSSetShaderResources(0, 1, &rawSRV);
        ctx->PSSetSamplers(4, 1, m_samLinearClamp.GetAddressOf());
        m_blurShader->Bind(ctx);
        fullscreenQuad->Render(ctx);

        return m_blurRT->GetSRV(); // 完成品を返す
    }

private:
    bool initNoiseTexture(ID3D11Device* pDevice);   
    bool initSamplers(ID3D11Device* pDevice);       
    bool initConstantBuffer(ID3D11Device* pDevice, float w, float h);
    void updateConstantBuffer(ID3D11DeviceContext* ctx); 

    RenderTarget* m_rawRT = nullptr;
    RenderTarget* m_blurRT = nullptr;
    Shader* m_shader = nullptr;
    Shader* m_blurShader = nullptr;

    ComPtr<ID3D11ShaderResourceView> m_noiseTextureSRV;
    ComPtr<ID3D11SamplerState> m_samPointClamp, m_samPointWrap, m_samLinearClamp;
    ComPtr<ID3D11Buffer> m_ssaoCB;
    SSAOParam m_paramData;
};