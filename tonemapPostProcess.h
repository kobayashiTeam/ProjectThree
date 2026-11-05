#pragma once
#include "postProcess.h"

// =====================================================================
// TonemapPostProcess：HDR → LDR の境目
//   t0：HDRシーン（PostProcess::Renderがバインド）
//   t1：Bloomの結果（nullptrなら未バインド扱い＝0が読める）
//   t2：カラーグレーディングの 3D LUT（nullptrなら未バインド。b5 の lutEnabled も 0 にしておく）
//   b5：PostProcessConstantBuffer（露出・方式・Bloomの強さ・LUTの有無）はRenderer側でバインド済みの前提
//
//   旧BloomCombinePostProcessの役割（Bloomを足す）も、ここに吸収している。
//   チェーンには入れず、Rendererが「HDRの段」と「LDRの段」の間で直接呼ぶ。
// =====================================================================
class TonemapPostProcess : public PostProcess {
public:
    TonemapPostProcess() = default;
    ~TonemapPostProcess() override = default;

    void SetBloomTexture(ID3D11ShaderResourceView* srv) { m_bloomSRV = srv; }
    void SetLutTexture(ID3D11ShaderResourceView* srv) { m_lutSRV = srv; }

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        if (!pContext) return;
        PostProcess::Render(pContext, sourceRT);
        pContext->PSSetShaderResources(1, 1, &m_bloomSRV);
        pContext->PSSetShaderResources(2, 1, &m_lutSRV);
    }

    // 描画後に t0〜t2 を外す
    //   t1：次フレームでBloomのmip0をRTとして使うため
    //   t2：LUT を焼き直すとき、CS が UAV としてつなぐため
    void Unbind(ID3D11DeviceContext* pContext) {
        ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
        pContext->PSSetShaderResources(0, 3, nullSRVs);
    }

private:
    ID3D11ShaderResourceView* m_bloomSRV = nullptr; // 借用（所有はBloomPass）
    ID3D11ShaderResourceView* m_lutSRV = nullptr;   // 借用（所有はColorGradingLut）
};
