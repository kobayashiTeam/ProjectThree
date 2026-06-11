// BlendState.h
#pragma once

#include <d3d11.h>
#include <wrl/client.h> // ComPtr のために必要

// 必要に応じて lib をコード側で明示的にリンク
#pragma comment(lib, "d3d11.lib")

class BlendState {
public:
    enum class Mode {
        None,       // 不透明
        Alpha,      // アルファブレンディング（半透明）
        Additive,   // 加算合成
    };

private:
    // Microsoft::WRL:: をつけるか、ファイル上部で using してください
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_state;

public:
    BlendState() = default;
    ~BlendState() = default;

    // 初期化（説明書を書いて、VRAMにステートオブジェクトを生成する）
    bool Initialize(ID3D11Device* device, Mode mode);

    // 適用（描画時にコンテキスト＝パイプラインへ通知する）
    void Bind(ID3D11DeviceContext* context);
};