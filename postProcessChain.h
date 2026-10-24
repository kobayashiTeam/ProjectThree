#pragma once
#include<d3d11.h>
#include<vector>
#include"shaderManager.h"
#include"graphicsCommon.h"
#include"renderTarget.h"
#include"postProcess.h"
#include"mesh.h"

class PostProcessChain {
public:
    template<typename T>
    T* AddEffect(ID3D11Device* device, ShaderID shaderID, bool activeByDefault) {
        T* effect = new T();
        // どのシェーダー（VS/PS か CS か）を取り出すかはエフェクト側が決める
        effect->InitializeFromID(device, shaderID);
        effect->SetActive(activeByDefault);
        m_effects.push_back(effect);
        return effect;
    }

    ~PostProcessChain() {
        for (PostProcess* fx : m_effects) {
            delete fx;
        }
        m_effects.clear();
    }

    // 戻り値：チェーンを抜けた後、最新の絵がどっちのRTに入っているか
    RenderTarget* Render(
        ID3D11DeviceContext* ctx,
        RenderTarget* input,
        RenderTarget* output,
        Mesh* fullscreenQuad) {

        RenderTarget* pCurrentInput = input;
        RenderTarget* pCurrentOutput = output;

        for (PostProcess* fx : m_effects) {
            if (!fx->IsActive()) continue;

            // 描き方（PSで四角形を描く／CSでDispatchする）はエフェクトに任せる
            // チェーンの仕事は「入力と出力を渡して、書けたら入れ替える」ことだけ
            if (!fx->Apply(ctx, pCurrentInput, pCurrentOutput, fullscreenQuad)) continue;

            std::swap(pCurrentInput, pCurrentOutput);
        }
        return pCurrentInput; // 最後のswapの後、"最新の絵"はinput側に来ている
    }

private:
    std::vector<PostProcess*> m_effects;
};