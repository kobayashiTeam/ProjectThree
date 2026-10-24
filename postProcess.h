#pragma once
#include <d3d11.h>
#include "shader.h"
#include"renderTarget.h"
#include "graphicsCommon.h"
#include "shaderManager.h"
#include "mesh.h"

class RenderTarget;

class PostProcess {
protected:
    Shader* m_pShader;         // 使用するポストプロセス用シェーダー
    ID3D11SamplerState* m_pSamplerLinear;  // ポストプロセス用のサンプラー（基本はCLAMP）
    bool m_isActive = true; //　このエフェクトを今適用するかどうか

public:
    PostProcess() : m_pShader(nullptr), m_pSamplerLinear(nullptr) {}
    virtual ~PostProcess() { Cleanup(); }

    // 初期化：シェーダーを受け取り、サンプラーを構築する
    virtual bool Initialize(ID3D11Device* pDevice, Shader* pShader) {
        if (!pShader) return false;
        m_pShader = pShader;

        // ポストプロセス用のサンプラー（LINEAR & CLAMP）を作成
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 画面端のにじみ防止
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.MinLOD = 0;
        sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

        HRESULT hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
        return SUCCEEDED(hr);
    }

    // 入力RTを読み込んで、出力RTに描画する
    // sourceRTが無効な場合は何もしない（呼び出し側で必ず有効なRTを渡す前提）
    virtual void Render(ID3D11DeviceContext* pContext, RenderTarget* sourceRT) {
        if (!pContext || !sourceRT) return;

        // 1. シェーダーのバインド
        if (m_pShader) m_pShader->Bind(pContext);

        // 2. 前の工程のテクスチャ（景色）とサンプラーをバインド
        ID3D11ShaderResourceView* srv = sourceRT->GetSRV();
        pContext->PSSetShaderResources(0, 1, &srv);
        pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
    }

    // Scene3のGバッファデバッグ表示用：RenderTargetを介さず、生のSRVを直接渡せる版
    // （GBufferPassはRenderTarget*ではなくSRVのgetterしか公開していないため）
    virtual void Render(ID3D11DeviceContext* pContext, ID3D11ShaderResourceView* sourceSRV) {
        if (!pContext || !sourceSRV) return;

        if (m_pShader) m_pShader->Bind(pContext);

        pContext->PSSetShaderResources(0, 1, &sourceSRV);
        pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
    }

    // チェーンから呼ばれる初期化の入口。ShaderID から自分に必要なシェーダーを取り出す
    // 既定：VS/PSの組を取り出して Initialize へ渡す。CS版のエフェクトはここを上書きしてCSを取り出す
    // （Initialize(Shader*) と名前を分けているのは、派生クラスで同じ名前の片方だけを上書きすると
    //   もう片方が見えなくなる（名前の隠蔽）ため）
    virtual bool InitializeFromID(ID3D11Device* pDevice, ShaderID id) {
        return Initialize(pDevice, ShaderManager::GetInstance().GetShader(id));
    }

    // チェーンから呼ばれる「1段ぶんの処理」：input を読んで output に書く
    // 既定：PS版の流れ（output をRTとしてつなぐ → シェーダーと入力をつなぐ → 全画面の四角形を描く）
    // CS版のエフェクトはここを上書きして、output をUAVとしてつなぎ Dispatch する
    // 戻り値：output に書けたか（false ならチェーンは入れ替えをせず、input をそのまま次の段へ渡す）
    virtual bool Apply(ID3D11DeviceContext* pContext, RenderTarget* input, RenderTarget* output,
        Mesh* fullscreenQuad) {
        if (!pContext || !input || !output || !fullscreenQuad) return false;
        output->Clear(pContext);
        output->Bind(pContext);
        Render(pContext, input);
        fullscreenQuad->Render(pContext);
        return true;
    }

    virtual void Cleanup() {
        m_pShader = nullptr; // 管理権はシェーダーマネージャーにあるため参照を切るだけ
        if (m_pSamplerLinear) { m_pSamplerLinear->Release(); m_pSamplerLinear = nullptr; }
    }

    //　有効・無効の切り替えアクセサ
    void SetActive(bool active) { m_isActive = active; }
    bool IsActive() const { return m_isActive; }
};