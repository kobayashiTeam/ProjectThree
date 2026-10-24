#pragma once
#include <d3d11.h>
#include "postProcess.h"
#include "shaderManager.h"
#include "renderTarget.h"

// =========================================================
// 色反転のCS（コンピュートシェーダー）版
//
// PS版（InversionPostProcess）との違い：
//   PS版：output をRTVとしてつなぎ、全画面の四角形を描く。
//         ラスタライザーがピクセルを数え上げ、1ピクセルにつき1回PSを呼ぶ
//   CS版：output をUAVとしてつなぎ、Dispatch する。
//         「何回呼ぶか」を自分で決める。8x8 = 64スレッドを1グループとして、
//         画面を覆うのに必要なグループ数を自分で計算して投げる
//
// 結果の絵はPS版と同じになるはず（PS版のサンプラーも、ピクセルの中心をちょうど読んでいるため）
// =========================================================
class InversionCSPostProcess : public PostProcess {
public:
    // 1グループの縦横のスレッド数。InversionCS.hlsl の THREAD_GROUP_SIZE と必ずそろえる
    static constexpr UINT THREAD_GROUP_SIZE = 8;

    // HLSL側の cbuffer InversionCB（b0）と同じ並び
    struct PerEffectCB {
        float intensity; // 反転の強さ（0.0 = 元の色、1.0 = 完全に反転）
        UINT  width;     // 書き込み先の大きさ。はみ出したスレッドを止めるのに使う
        UINT  height;
        float pad;
    };
    static_assert(sizeof(PerEffectCB) % 16 == 0, "cbuffer size must be a multiple of 16 bytes");

    InversionCSPostProcess() { m_cbData.intensity = 1.0f; }
    ~InversionCSPostProcess() override = default;

    void SetIntensity(float intensity) { m_cbData.intensity = intensity; }

    // VS/PSの組ではなく、CSを取り出す
    bool InitializeFromID(ID3D11Device* pDevice, ShaderID id) override {
        m_pCS = ShaderManager::GetInstance().GetCS(id);
        if (!m_pCS) return false;

        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(PerEffectCB);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, m_pConstantBuffer.GetAddressOf());
        return SUCCEEDED(hr);
    }

    bool Apply(ID3D11DeviceContext* pContext, RenderTarget* input, RenderTarget* output,
        Mesh* /*fullscreenQuad*/) override {
        if (!pContext || !input || !output || !m_pCS) return false;
        ID3D11ShaderResourceView* srv = input->GetSRV();
        ID3D11UnorderedAccessView* uav = output->GetUAV();
        if (!srv || !uav) return false; // UAVなしで作ったRTが渡された（入れ替えずに次の段へ）

        // 1. つなぎっぱなしの「書き込み口」と「読み込み口」を外す
        //    同じテクスチャを、読み込み（SRV）と書き込み（RTV/UAV）に同時につなぐことはできない。
        //    ・input は直前の段の出力なので、まだRTVとしてOMにつながっている
        //      → このままSRVとしてつなぐと、ランタイムがSRVのほうを nullptr にしてしまう
        //    ・output は直前の段の入力なので、まだPSのt0にSRVとしてつながっている
        ID3D11ShaderResourceView* nullSRV = nullptr;
        pContext->OMSetRenderTargets(0, nullptr, nullptr);
        pContext->PSSetShaderResources(0, 1, &nullSRV);

        // 2. 定数バッファ（大きさは毎回 output から取る。値の持ち主を output の1か所にする）
        m_cbData.width = output->Width();
        m_cbData.height = output->Height();
        pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);

        // 3. CSのスロットにつなぐ（スロット表はステージごとに別。CSの b0 と PSの b0 は別物）
        ID3D11Buffer* cb = m_pConstantBuffer.Get();
        pContext->CSSetShader(m_pCS, nullptr, 0);
        pContext->CSSetConstantBuffers(0, 1, &cb);
        pContext->CSSetShaderResources(0, 1, &srv);
        pContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

        // 4. グループ数は切り上げ。1280x720 なら 160x90 グループでちょうど割り切れるが、
        //    割り切れない大きさ（例：幅1366）でも右端の列が欠けないよう、(大きさ + 7) / 8 で割る。
        //    はみ出したスレッドはシェーダー側で止める
        UINT groupsX = (m_cbData.width + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
        UINT groupsY = (m_cbData.height + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
        pContext->Dispatch(groupsX, groupsY, 1);

        // 5. 後片付け：使い終わったら外す
        //    外さないと、次の段で output をSRVとして読もうとしたとき、
        //    まだUAVとしてつながっているせいでSRVのほうが nullptr にされる
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        pContext->CSSetShaderResources(0, 1, &nullSRV);
        pContext->CSSetShader(nullptr, nullptr, 0);
        return true;
    }

private:
    ID3D11ComputeShader* m_pCS = nullptr; // 所有はShaderManager
    ComPtr<ID3D11Buffer> m_pConstantBuffer;
    PerEffectCB m_cbData = {};
};
