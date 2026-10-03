#pragma once
#include"postProcess.h"

class VignettePostProcess : public PostProcess {
public:
    // VignetteShader.hlsl の PerEffectCB(b2) と同じ並びにする
    // （以前は radius / softness がdummyのまま未初期化で、シェーダー側に不定値が渡っていた）
    struct PerEffectCB {
        float intensity = 0.6f; // ビネットの強さ (0.0 = 通常, 1.0 = 完全なビネット)
        float radius = 0.75f;   // 中心からこの距離までは暗くならない（UV単位）
        float softness = 0.45f; // radiusから内側へ、どれだけの幅でなめらかに暗くするか
        float dummy = 0.0f;     // 16バイトアライメント用のパディング
    };

private:
    ID3D11Buffer* m_pConstantBuffer = nullptr;
    PerEffectCB   m_cbData;

public:
    VignettePostProcess() = default;
    ~VignettePostProcess() override { if (m_pConstantBuffer) m_pConstantBuffer->Release(); }

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

    // 外部からビネットの強さを変えるアクセサ
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
