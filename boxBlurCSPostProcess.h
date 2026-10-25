#pragma once
#include <d3d11.h>
#include <memory>
#include <algorithm>
#include "postProcess.h"
#include "shaderManager.h"
#include "renderTarget.h"

// =========================================================
// ボックスブラーのCS版（分離ブラー：横 → 縦）
//
// PS版（SimpleBoxBlurPostProcess）との違い：
//   PS版：1回の描画で (2r+1)×(2r+1) 個を平均する（半径8なら 289 回読む）
//   CS版：横に (2r+1) 個 → 縦に (2r+1) 個の2回に分ける（半径8なら 17＋17 ＝ 34 回）
//         ボックスブラーは「横に平均してから縦に平均」しても、2Dでまとめて平均しても同じ結果になる
//
// 同じクラスで2つの版を作る（どちらにするかは ShaderID で決める）
//   ShaderID::BoxBlurCS         ：近所を groupshared から読む
//   ShaderID::BoxBlurCSNoShared ：近所を毎回テクスチャから読む
//
// 横の結果を置く中間のRTは、このエフェクト自身が持つ（案1）
//   → チェーンから渡される input は読むだけ、という約束を守る
// =========================================================
class BoxBlurCSPostProcess : public PostProcess {
public:
    // BoxBlurCS.hlsl の GROUP_SIZE / MAX_RADIUS と必ずそろえる
    static constexpr UINT GROUP_SIZE = 256; // 1グループ ＝ 1本の列の、連続した 256 画素
    static constexpr int  MAX_RADIUS = 16;  // 共有の配列の大きさを決めるので、これより大きくできない

    // HLSL側の cbuffer BoxBlurCB（b0）と同じ並び
    struct PerEffectCB {
        UINT width;      // 画像の大きさ（向きによらず、画像そのものの大きさ）
        UINT height;
        int  radius;
        UINT horizontal; // 1 = 横の回、0 = 縦の回。Dispatch の前に毎回書き換える
    };
    static_assert(sizeof(PerEffectCB) % 16 == 0, "cbuffer size must be a multiple of 16 bytes");

    BoxBlurCSPostProcess() = default;
    ~BoxBlurCSPostProcess() override = default;

    void SetRadius(int radius) { m_radius = std::clamp(radius, 1, MAX_RADIUS); }

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
        if (!input->GetSRV() || !output->GetUAV()) return false; // UAVなしのRTが渡された（入れ替えずに次の段へ）
        if (!ensureTempRT(pContext, output)) return false;

        // 1. つなぎっぱなしの口を外す（Inversion のCS版と同じ理由）
        //    input はまだRTVとしてOMに、output はまだSRVとしてPSのt0につながっている
        ID3D11ShaderResourceView* nullSRV = nullptr;
        pContext->OMSetRenderTargets(0, nullptr, nullptr);
        pContext->PSSetShaderResources(0, 1, &nullSRV);

        m_cbData.width = output->Width();
        m_cbData.height = output->Height();
        m_cbData.radius = m_radius;

        pContext->CSSetShader(m_pCS, nullptr, 0);

        // 2. 横の回：input → 中間のRT
        runPass(pContext, input->GetSRV(), m_tempRT->GetUAV(), true);
        // 3. 縦の回：中間のRT → output
        //    2回の Dispatch の切れ目が「横を全部終えてから縦」の順番を守る
        //    （1回の Dispatch の中では、グループ同士は待ち合わせができない）
        runPass(pContext, m_tempRT->GetSRV(), output->GetUAV(), false);

        pContext->CSSetShader(nullptr, nullptr, 0);
        return true;
    }

private:
    // 1回ぶん（横 or 縦）の Dispatch。最後に必ず口を外す
    void runPass(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* srv,
        ID3D11UnorderedAccessView* uav, bool horizontal) {
        m_cbData.horizontal = horizontal ? 1u : 0u;
        pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &m_cbData, 0, 0);

        ID3D11Buffer* cb = m_pConstantBuffer.Get();
        pContext->CSSetConstantBuffers(0, 1, &cb);
        pContext->CSSetShaderResources(0, 1, &srv);
        pContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

        // グループの数：x は列の長さを 256 で切り上げた数、y は列の本数
        //   1280x720 の横の回：(1280 + 255) / 256 ＝ 5 グループ × 720 本
        //   1280x720 の縦の回：( 720 + 255) / 256 ＝ 3 グループ × 1280 本（3×256 ＝ 768 なので 48 画素はみ出す）
        UINT lineLength = horizontal ? m_cbData.width : m_cbData.height;
        UINT lineCount = horizontal ? m_cbData.height : m_cbData.width;
        pContext->Dispatch((lineLength + GROUP_SIZE - 1) / GROUP_SIZE, lineCount, 1);

        // 外す：横の回で書いた中間のRTを、縦の回で SRV として読むため
        //       （UAVにつながったままだと、SRVのほうが黙って nullptr にされる）
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        ID3D11ShaderResourceView* nullSRV = nullptr;
        pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        pContext->CSSetShaderResources(0, 1, &nullSRV);
    }

    // 中間のRTを output と同じ大きさ・形式で用意する
    // InitializeFromID の時点では大きさがわからないので、最初の Apply で作る
    // （大きさが変わったときも作り直す）
    bool ensureTempRT(ID3D11DeviceContext* pContext, RenderTarget* output) {
        if (m_tempRT && m_tempRT->Width() == output->Width() && m_tempRT->Height() == output->Height()) {
            return true;
        }
        D3D11_TEXTURE2D_DESC desc = {};
        output->GetTexture()->GetDesc(&desc);

        ComPtr<ID3D11Device> device;
        pContext->GetDevice(device.GetAddressOf());

        auto rt = std::make_unique<RenderTarget>();
        // 深度は不要、UAVは必要（横の回で書き込むため）
        if (!rt->Initialize(device.Get(), output->Width(), output->Height(), desc.Format, false, true)) {
            return false;
        }
        m_tempRT = std::move(rt);
        return true;
    }

    ID3D11ComputeShader* m_pCS = nullptr; // 所有はShaderManager
    ComPtr<ID3D11Buffer> m_pConstantBuffer;
    PerEffectCB m_cbData = {};
    int m_radius = 2;
    std::unique_ptr<RenderTarget> m_tempRT; // 横の結果を置く中間のRT（案1）
};
