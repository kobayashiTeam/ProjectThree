#include<d3d11.h>
#include"renderTarget.h"
#include"shaderManager.h"
#include"mesh.h"
#include"graphicsCommon.h"


class SSAOPass {
public:
    bool Initialize(ID3D11Device* device, UINT width, UINT height) {
        // 解像度ごとに「生成用」と「ブラー用」の2枚を持つ（[0]=全解像度、[1]=半分）
        // 比較用に両方を常に持っておき、F7で使う方を切り替える（作り直さないので切り替えが一瞬で済む）
        const UINT sizes[2][2] = {
            { width,             height },
            { (width + 1) / 2,   (height + 1) / 2 }, // 奇数のとき1ピクセル足りなくならないよう切り上げ
        };
        for (int i = 0; i < 2; ++i) {
            m_rawRT[i] = new RenderTarget();
            if (!m_rawRT[i]->Initialize(device, sizes[i][0], sizes[i][1], DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
            m_blurRT[i] = new RenderTarget();
            if (!m_blurRT[i]->Initialize(device, sizes[i][0], sizes[i][1], DXGI_FORMAT_R16G16B16A16_FLOAT)) return false;
        }

        if (!initNoiseTexture(device)) return false;
        if (!initSamplers(device)) return false;
        if (!initConstantBuffer(device)) return false;

        m_shader = ShaderManager::GetInstance().GetShader(ShaderID::SSAO);
        m_blurShader = ShaderManager::GetInstance().GetShader(ShaderID::SSAOBlur);
        return true;
    }

    ~SSAOPass() {
        for (int i = 0; i < 2; ++i) {
            delete m_rawRT[i];
            delete m_blurRT[i];
        }
    }

    // 計測で見比べるためのスイッチ（フレームをまたいで保持する）
    void ToggleHalfRes() { m_halfRes = !m_halfRes; }
    bool IsHalfRes() const { return m_halfRes; }

    // GBufferPassから直接SRVを受け取る形にすることで依存関係を引数で明示する
    // 返すSRVは半分の解像度のこともある。元の大きさに戻すのは、読む側（ライティング）のLinearサンプラーに任せる
    ID3D11ShaderResourceView* Execute(ID3D11DeviceContext* ctx,
        ID3D11ShaderResourceView* gbufferNormalSRV,
        ID3D11ShaderResourceView* gbufferPositionSRV,
        Mesh* fullscreenQuad)
    {
        const int idx = m_halfRes ? 1 : 0;
        RenderTarget* rawRT = m_rawRT[idx];
        RenderTarget* blurRT = m_blurRT[idx];

        // 1. SSAO生成（BindがビューポートもこのRTの大きさに合わせる。G-BufferはUVで読むので全解像度のまま）
        rawRT->Clear(ctx);
        rawRT->Bind(ctx);

        ID3D11ShaderResourceView* inputs[3] = {
            gbufferNormalSRV, gbufferPositionSRV, m_noiseTextureSRV.Get()
        };
        ctx->PSSetShaderResources(0, 3, inputs);

        ID3D11SamplerState* samplers[] = { m_samPointClamp.Get(), m_samPointWrap.Get() };
        ctx->PSSetSamplers(4, 2, samplers);

        // ノイズの敷き詰め回数は「書き込む先」のピクセル数で決める（1ピクセル＝ノイズ1マス）
        updateConstantBuffer(ctx, (float)rawRT->Width(), (float)rawRT->Height());
        m_shader->Bind(ctx);
        fullscreenQuad->Render(ctx);

        // 2. ブラー（GetDimensionsで入力の大きさを見るので、シェーダーは変更なし）
        blurRT->Clear(ctx);
        blurRT->Bind(ctx);
        auto* rawSRV = rawRT->GetSRV();
        ctx->PSSetShaderResources(0, 1, &rawSRV);
        ctx->PSSetSamplers(4, 1, m_samLinearClamp.GetAddressOf());
        m_blurShader->Bind(ctx);
        fullscreenQuad->Render(ctx);

        return blurRT->GetSRV(); // 完成品を返す
    }

private:
    bool initNoiseTexture(ID3D11Device* pDevice);
    bool initSamplers(ID3D11Device* pDevice);
    bool initConstantBuffer(ID3D11Device* pDevice);
    void updateConstantBuffer(ID3D11DeviceContext* ctx, float targetW, float targetH);

    RenderTarget* m_rawRT[2] = { nullptr, nullptr };
    RenderTarget* m_blurRT[2] = { nullptr, nullptr };
    bool m_halfRes = false; // 起動時は今までどおり全解像度
    Shader* m_shader = nullptr;
    Shader* m_blurShader = nullptr;

    ComPtr<ID3D11ShaderResourceView> m_noiseTextureSRV;
    ComPtr<ID3D11SamplerState> m_samPointClamp, m_samPointWrap, m_samLinearClamp;
    ComPtr<ID3D11Buffer> m_ssaoCB;
    SSAOParam m_paramData;
};