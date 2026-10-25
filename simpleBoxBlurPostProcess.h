#pragma once
#include"postProcess.h"

// ボックスブラーのPS版：1回の描画で (2r+1)×(2r+1) 個を平均する（2Dのまま、分離しない）
// CS版（BoxBlurCSPostProcess）と同じ半径・同じ結果になるようにそろえてあり、ms の比べ合いの基準に使う
class SimpleBoxBlurPostProcess : public PostProcess {
public:
    // HLSL側の cbuffer PerEffectCB（b2）と同じ並び
    // （以前は C++ 側が {intensity, dummy[3]} のままで、HLSL 側の texelSize に 0 が入り、ぼけていなかった）
    struct PerEffectCB {
        int   radius;       // 半径。CS版と同じく 1 から 16 の範囲で使う
        float texelSize[2]; // (1/幅, 1/高さ)。Render で入力の大きさから毎回作る
        float pad;          // 16バイトアライメント用のパディング
    };
    static_assert(sizeof(PerEffectCB) % 16 == 0, "cbuffer size must be a multiple of 16 bytes");

private:
    ID3D11Buffer* m_pConstantBuffer = nullptr;
    PerEffectCB   m_cbData = {};

public:
    SimpleBoxBlurPostProcess() { m_cbData.radius = 2; }
    ~SimpleBoxBlurPostProcess() override { if (m_pConstantBuffer) m_pConstantBuffer->Release(); }

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

    void SetRadius(int radius) { m_cbData.radius = (radius < 1) ? 1 : (radius > 16 ? 16 : radius); }

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        // 1. 親クラスの基本バインド（シェーダー、テクスチャ、サンプラー）を呼ぶ
        PostProcess::Render(pContext, sourceRT);
        if (!sourceRT) return;

        // 2. 1ピクセルぶんのUVの大きさは、読む側（入力）の大きさから作る
        m_cbData.texelSize[0] = 1.0f / (float)sourceRT->Width();
        m_cbData.texelSize[1] = 1.0f / (float)sourceRT->Height();

        // 3. 自分専用の定数バッファを更新してスロット2にバインド
        if (m_pConstantBuffer) {
            pContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &m_cbData, 0, 0);
            pContext->PSSetConstantBuffers(2, 1, &m_pConstantBuffer);
        }
    }
};
