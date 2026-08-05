#include"postProcess.h"

class SepiaPostProcess : public PostProcess {
public:
    struct PerEffectCB {
        float intensity; // セピアの強さ (0.0 = 通常, 1.0 = 完全なセピア)
        float dummy[3];  // 16バイトアライメント用のパディング
    };

private:
    ID3D11Buffer* m_pConstantBuffer = nullptr;
    PerEffectCB   m_cbData;

public:
    SepiaPostProcess() { m_cbData.intensity = 1.0f; }
    ~SepiaPostProcess() override { if (m_pConstantBuffer) m_pConstantBuffer->Release(); }

    bool Initialize(ID3D11Device* pDevice, Shader* pShader) override {
        if (!PostProcess::Initialize(pDevice, pShader)) return false;

        // エフェクト専用の定数バッファ（スロット2用）を作成
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(PerEffectCB);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer);
        return SUCCEEDED(hr);
    }

    // 外部からセピアの強さを変えるアクセサ
    void SetIntensity(float intensity) { m_cbData.intensity = intensity; }

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        // 1. 親クラスの基本バインド（シェーダー、テクスチャ、サンプラー）を呼ぶ
        PostProcess::Render(pContext, sourceRT);

        // 2. 自分専用の定数バッファを更新してスロット2にバインド
        if (m_pConstantBuffer) {
            pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &m_cbData, 0, 0);
            pContext->PSSetConstantBuffers(2, 1, &m_pConstantBuffer);
        }
    }
};