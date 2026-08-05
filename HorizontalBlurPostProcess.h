#pragma once
#include "postProcess.h"

// 横ブラー用ポストプロセスクラス
class HorizontalBlurPostProcess : public PostProcess {
public:
    HorizontalBlurPostProcess() = default;
    ~HorizontalBlurPostProcess() override = default;

    // Initialize は基底クラスのものをそのまま利用できるため、
    // 特に追加の処理がなければ、わざわざオーバーライドして書かなくても大丈夫です。
    // (Renderer側で m_horizontalBlurEffect->Initialize(pDevice, pShader) を呼べば基底のものが走ります)

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        // 基底クラスの Render を呼び出す
        // これだけで自動的に：
        // 1. シェーダーのバインド
        // 2. sourceRT のテクスチャを「スロット t0 (register(t0))」に設定
        // 3. クランプサンプラーを「スロット s0 (register(s0))」に設定
        PostProcess::Render(pContext, sourceRT);
    }
};