#pragma once

#include"model.h"
#include<algorithm>
#include"blendStates.h"
#include"graphicsCommon.h"

// ソート担当クラスの簡易イメージ
class RenderQueue {
public:
    struct RenderCommand {
        Model* pModel;
        float depth; // カメラからの距離
    };

private:
    // BlendModeの数（Count = 3）だけ、コマンドの配列（バッファ）を用意する
    // m_queues[0] が不透明、m_queues[1] が半透明、m_queues[2] が加算...となる
    std::vector<RenderCommand> m_queues[static_cast<int>(BlendMode::Count)];

    // 外部から借りてくるブレンドステートのポインタ配列
    //BlendState* m_pBlendStates[static_cast<int>(BlendMode::Count)] = {};

public:
	RenderQueue() = default;
    // ① 初期化時（またはメイン側でステートを作った時）に、ポインタの参照を登録しておく
    /*void RegisterBlendState(BlendMode mode, BlendState* pState) {
        m_pBlendStates[static_cast<int>(mode)] = pState;
    }*/

    // ② 登録時は、どのブレンドタイプで描画したいかを指定してキューに入れる
    void Submit(Model* pModel, float depth, BlendMode mode) {
        m_queues[static_cast<int>(mode)].push_back({ pModel, depth });
    }

    // ③ 実行（描画）
    void Execute(ID3D11DeviceContext* pContext, ID3D11Buffer* pPerFrameCB,BlendStates* pBlendStates) {
        int opaqueIdx = static_cast<int>(BlendMode::Opaque);
        int alphaIdx = static_cast<int>(BlendMode::AlphaBlend);
        int addIdx = static_cast<int>(BlendMode::Additive);

        // 1. ソート処理
        // 不透明は手前から奥（昇順）
        std::sort(m_queues[opaqueIdx].begin(), m_queues[opaqueIdx].end(),
            [](const RenderCommand& a, const RenderCommand& b) { return a.depth < b.depth; });

        // 半透明と加算は奥から手前（降順）
        auto backToFront = [](const RenderCommand& a, const RenderCommand& b) { return a.depth > b.depth; };
        std::sort(m_queues[alphaIdx].begin(), m_queues[alphaIdx].end(), backToFront);
        std::sort(m_queues[addIdx].begin(), m_queues[addIdx].end(), backToFront);

        // 2. 順次描画（登録されたステートを自動でバインドしながらループ）
        for (int i = 0; i < static_cast<int>(BlendMode::Count); ++i) {
            if (m_queues[i].empty()) continue;

            // 事前に登録しておいた対応するブレンドステートをバインド（参照してBind）
            if (pBlendStates) {
                pBlendStates->Bind(pContext,static_cast<BlendMode>(i));
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