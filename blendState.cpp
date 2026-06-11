#include"blendState.h"

// BlendState.cpp の一部イメージ
bool BlendState::Initialize(ID3D11Device* device, Mode mode) {
    D3D11_BLEND_DESC desc;
    ZeroMemory(&desc, sizeof(desc));

    // 全モード共通の基本設定（例：1番目の画面に対して適用）
    desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    // 条件（モード）に応じて説明書を書き換える
    if (mode == Mode::Alpha) {
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    }
    else if (mode == Mode::Additive) {
        // 加算合成用のカスタマイズ（光のエフェクト用など）
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // または D3D11_BLEND_ONE
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;       // 背景にそのまま足す
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        // (Alpha側は省略、必要に応じて設定)
    }
    else {
        // None（ブレンド無効）
        desc.RenderTarget[0].BlendEnable = FALSE;
    }

    // デバイス（ファクトリ）に説明書を渡してVRAM上に生成
    HRESULT hr = device->CreateBlendState(&desc, m_state.GetAddressOf());
    return SUCCEEDED(hr);
}

void BlendState::Bind(ID3D11DeviceContext* context) {
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    UINT sampleMask = 0xffffffff;

    // パイプライン（コンテキスト）に「このステートでいけ」と通知する
    context->OMSetBlendState(m_state.Get(), blendFactor, sampleMask);
}