#pragma once

#include"model.h"
#include<algorithm>
#include"blendState.h"

// ソート担当クラスの簡易イメージ
class RenderQueue {
public:
    // 種類の定義
    enum class BlendType {
        Opaque,       // 不透明（None）
        AlphaBlend,   // 半透明
        Additive,     // 加算合成（エフェクト用）
        Count         // バッファの数（自動的に 3 になる）
    };
    struct RenderCommand {
        Model* pModel;
        float depth; // カメラからの距離
    };

private:
    // BlendTypeの数（Count = 3）だけ、コマンドの配列（バッファ）を用意する
    // m_queues[0] が不透明、m_queues[1] が半透明、m_queues[2] が加算...となる
    std::vector<RenderCommand> m_queues[static_cast<int>(BlendType::Count)];

    // 外部から借りてくるブレンドステートのポインタ配列
    BlendState* m_pBlendStates[static_cast<int>(BlendType::Count)] = {};

public:
	RenderQueue() = default;
    // ① 初期化時（またはメイン側でステートを作った時）に、ポインタの参照を登録しておく
    void RegisterBlendState(BlendType type, BlendState* pState) {
        m_pBlendStates[static_cast<int>(type)] = pState;
    }

    // ② 登録時は、どのブレンドタイプで描画したいかを指定してキューに入れる
    void Submit(Model* pModel, float depth, BlendType type) {
        m_queues[static_cast<int>(type)].push_back({ pModel, depth });
    }

    // ③ 実行（描画）
    void Execute(ID3D11DeviceContext* pContext, ID3D11Buffer* pPerFrameCB) {
        int opaqueIdx = static_cast<int>(BlendType::Opaque);
        int alphaIdx = static_cast<int>(BlendType::AlphaBlend);
        int addIdx = static_cast<int>(BlendType::Additive);

        // 1. ソート処理
        // 不透明は手前から奥（昇順）
        std::sort(m_queues[opaqueIdx].begin(), m_queues[opaqueIdx].end(),
            [](const RenderCommand& a, const RenderCommand& b) { return a.depth < b.depth; });

        // 半透明と加算は奥から手前（降順）
        auto backToFront = [](const RenderCommand& a, const RenderCommand& b) { return a.depth > b.depth; };
        std::sort(m_queues[alphaIdx].begin(), m_queues[alphaIdx].end(), backToFront);
        std::sort(m_queues[addIdx].begin(), m_queues[addIdx].end(), backToFront);

        // 2. 順次描画（登録されたステートを自動でバインドしながらループ）
        for (int i = 0; i < static_cast<int>(BlendType::Count); ++i) {
            if (m_queues[i].empty()) continue;

            // 事前に登録しておいた対応するブレンドステートをバインド（参照してBind）
            if (m_pBlendStates[i]) {
                m_pBlendStates[i]->Bind(pContext);
            }

            // そのブレンドタイプに溜まっているモデルを全描画
            for (const auto& cmd : m_queues[i]) {
                cmd.pModel->Draw(pContext, pPerFrameCB);
            }

            // 描画が終わったらそのキューをクリア
            m_queues[i].clear();
        }
    }
};