#pragma once

#include"model.h"
#include<algorithm>
#include"blendStates.h"
#include"graphicsCommon.h"
#include<DirectXCollision.h>

// 描画コマンドを管理し、描画順を制御するクラス
class RenderQueue {
public:
    struct RenderCommand {
        Model* pModel;
        float depth; // カメラからの距離
        Shader* pOverrideShader = nullptr;
        // ⑤視錐台カリング用：Submit時点のワールドAABB
        // （1フレームの中で、同じ物体を何パスも描くので、計算はSubmitで1回だけにする）
        DirectX::BoundingBox bounds;
        bool hasBounds = false; // false＝大きさが分からない → 常に描く
    };

    // ⑤視錐台カリングの集計（画面表示用）。total＝登録数、drawn＝実際に描いた数
    struct CullStats {
        int total = 0;
        int drawn = 0;
    };

private:
    // BlendModeごとに描画コマンドを管理するキュー
    std::vector<RenderCommand> m_queues[static_cast<int>(BlendMode::Count)];


public:
    RenderQueue() = default;

    // 登録時は、どのブレンドタイプで描画したいかを指定してキューに入れる
    void Submit(Model* pModel, float depth, BlendMode mode) {
        RenderCommand cmd;
        cmd.pModel = pModel;
        cmd.depth = depth;
        cmd.hasBounds = pModel->GetWorldBounds(cmd.bounds);
        m_queues[static_cast<int>(mode)].push_back(cmd);
    }

    // 実行（描画）
    // pCullFrustum：このパスで「見える範囲」（ワールド空間）。nullptr ならカリングしない（全部描く）
    //   → どの範囲で間引くかはパスごとに違う（本描画＝カメラ、影＝ライトから見た範囲）ので、
    //     キューには持たせず、呼び出し側が毎回渡す
    // pStats：渡されたら、登録数と描いた数を足し込む
    void Execute(ID3D11DeviceContext* pContext,
        ID3D11Buffer* pPerFrameCB,
        BlendStates* pBlendStates,
        bool isAfterClear,
        const DirectX::BoundingFrustum* pCullFrustum = nullptr,
        CullStats* pStats = nullptr) {
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

            // ブレンドモードに対応するステートを設定
            if (pBlendStates) {
                pBlendStates->Bind(pContext, static_cast<BlendMode>(i));
            }

            // そのブレンドタイプに溜まっているモデルを全描画（視錐台の外は飛ばす）
            for (const auto& cmd : m_queues[i]) {
                if (pStats) pStats->total++;
                if (pCullFrustum && cmd.hasBounds && !pCullFrustum->Intersects(cmd.bounds)) {
                    continue; // 箱が視錐台と1点も重ならない＝画面に1ピクセルも映らない
                }
                if (pStats) pStats->drawn++;

                if (!cmd.pOverrideShader) { cmd.pModel->Draw(pContext, pPerFrameCB); }
                else { cmd.pModel->DrawGeometryOnly(pContext, pPerFrameCB); }
            }

            // 描画が終わったらそのキューをクリア
            if (isAfterClear)
                m_queues[i].clear();
        }
    }

    // 各modelにシェーダをbindさせずvertex,index描画だけさせる
    void ExecuteGeometryOnly(ID3D11DeviceContext* pContext,
        ID3D11Buffer* pPerFrameCB,
        BlendStates* pBlendStates,
        bool isAfterClear) {
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
                pBlendStates->Bind(pContext, static_cast<BlendMode>(i));
            }

            // そのブレンドタイプに溜まっているモデルを全描画
            for (const auto& cmd : m_queues[i]) {
                cmd.pModel->DrawGeometryOnly(pContext, pPerFrameCB);
            }

            // 描画が終わったらそのキューをクリア
            if (isAfterClear)
                m_queues[i].clear();
        }
    }

    // このパス（Opaque/Transparentなど）に、今フレーム1つでも登録があるか
    // （Execute(..., true)でクリアされる前に聞くこと）
    bool IsEmpty() const {
        for (const auto& q : m_queues) {
            if (!q.empty()) return false;
        }
        return true;
    }

    void SetOverrideVS(Shader* pVS)
    {
        int opaqueIdx = static_cast<int>(BlendMode::Opaque);
        for (auto& cmd : m_queues[opaqueIdx])
        {
            cmd.pOverrideShader = pVS;
        }
    }
};