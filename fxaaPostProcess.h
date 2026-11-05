#pragma once
#include "postProcess.h"

// =====================================================================
// FxaaPostProcess：画面の絵だけを見て輪郭のジャギーをぼかす（FxaaShader.hlsl）
//
//   置き場所はTonemapの直後（LDRの段の最初）。チェーンには入れず、Rendererが直接呼ぶ。
//   （チェーンは「どのエフェクトをONにするか」をシーンがBeginFrameごとに決める作りだが、
//     FXAAはSSAOの解像度と同じく全シーン共通のスイッチなので、順番も固定で先頭に置きたい）
//
//   入力の大きさは HLSL 側で GetDimensions で調べるので、C++からは渡さない。
//   b2：デバッグ表示（輪郭として扱ったピクセルを色で見せる）のON/OFFだけ
// =====================================================================
class FxaaPostProcess : public PostProcess {
public:
    // FxaaShader.hlsl の PerEffectCB(b2) と同じ並びにする
    struct PerEffectCB {
        int   edgeView = 0;          // 0=通常、1=輪郭のデバッグ表示
        float padding[3] = {};       // 16バイトアライメント用
    };

private:
    ID3D11Buffer* m_pConstantBuffer = nullptr;
    PerEffectCB   m_cbData;

public:
    FxaaPostProcess() = default;
    ~FxaaPostProcess() override { if (m_pConstantBuffer) m_pConstantBuffer->Release(); }

    bool Initialize(ID3D11Device* pDevice, Shader* pShader) override {
        if (!PostProcess::Initialize(pDevice, pShader)) return false;

        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(PerEffectCB);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer);
        return SUCCEEDED(hr);
    }

    void SetEdgeView(bool isOn) { m_cbData.edgeView = isOn ? 1 : 0; }

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        // 1. 親クラスの基本バインド（シェーダー、入力のテクスチャ t0、LINEAR・CLAMPのサンプラー s0）
        PostProcess::Render(pContext, sourceRT);

        // 2. 自分専用の定数バッファを更新して b2 にバインド
        if (m_pConstantBuffer) {
            pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &m_cbData, 0, 0);
            pContext->PSSetConstantBuffers(2, 1, &m_pConstantBuffer);
        }
    }
};
