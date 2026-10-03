#pragma once
#include "postProcess.h"

// =====================================================================
// TonemapPostProcess：HDR → LDR の境目
//   t0：HDRシーン（PostProcess::Renderがバインド）
//   t1：Bloomの結果（nullptrなら未バインド扱い＝0が読める）
//   b5：PostProcessConstantBuffer（露出・方式・Bloomの強さ）はRenderer側でバインド済みの前提
//
//   旧BloomCombinePostProcessの役割（Bloomを足す）も、ここに吸収している。
//   チェーンには入れず、Rendererが「HDRの段」と「LDRの段」の間で直接呼ぶ。
// =====================================================================
class TonemapPostProcess : public PostProcess {
public:
    TonemapPostProcess() = default;
    ~TonemapPostProcess() override = default;

    void SetBloomTexture(ID3D11ShaderResourceView* srv) { m_bloomSRV = srv; }

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        if (!pContext) return;
        PostProcess::Render(pContext, sourceRT);
        pContext->PSSetShaderResources(1, 1, &m_bloomSRV);
    }

    // 描画後に t1 を外す（次フレームでBloomのmip0をRTとして使うため）
    void Unbind(ID3D11DeviceContext* pContext) {
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        pContext->PSSetShaderResources(0, 2, nullSRVs);
    }

private:
    ID3D11ShaderResourceView* m_bloomSRV = nullptr; // 借用（所有はBloomPass）
};
