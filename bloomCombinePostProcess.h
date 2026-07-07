#pragma once
#include "postProcess.h"
#include <wrl/client.h> // Microsoft::WRL::ComPtr 用

//using Microsoft::WRL::ComPtr;

// Bloom合成用ポストプロセスクラス
class BloomCombinePostProcess : public PostProcess {
private:
    // 事前処理で作成した、完全にボケ上がった高輝度テクスチャのビュー(t1用)
    ComPtr<ID3D11ShaderResourceView> m_pBrightBlurSRV = nullptr;

public:
    BloomCombinePostProcess() = default;
    ~BloomCombinePostProcess() override = default;

    // 定数バッファなど個別の生成物が無いため、Initializeは基底クラスのものをそのまま流用します
    bool Initialize(ID3D11Device* pDevice, Shader* pShader) override {
        return PostProcess::Initialize(pDevice, pShader);
    }

    // ★【重要】チェーンが回る前に、Renderer側からボケ画像をこの窓口に放り込んでもらう
    void SetBrightBlurTexture(ID3D11ShaderResourceView* srv) {
        m_pBrightBlurSRV = srv;
    }

    void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) override {
        if (!pContext) return;

        // 1. 親クラスの基本バインドを呼ぶ
        // これにより自動的に：
        // ・合成用シェーダーのバインド
        // ・sourceRT (これまでのシーン) を「スロット t0 (register(t0))」にバインド
        // ・クランプサンプラーを「スロット s0 (register(s0))」にバインド
        PostProcess::Render(pContext, sourceRT);

        // 2. 【このクラス固有の処理】
        // 事前にセットしておいた「ボケ画像」を「スロット t1 (register(t1))」にバインドする！
        if (m_pBrightBlurSRV) {
            ID3D11ShaderResourceView* srvArray[] = { m_pBrightBlurSRV.Get() };
            pContext->PSSetShaderResources(1, 1, srvArray); // 第1引数「1」がスロット t1 を指します
        }
    }
};