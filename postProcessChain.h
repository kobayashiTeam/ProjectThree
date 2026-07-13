// PostProcessChain.h
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
        effect->Initialize(device, ShaderManager::GetInstance().GetShader(shaderID));
        effect->SetActive(activeByDefault);
        m_effects.push_back(effect);
        return effect;
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

            pCurrentOutput->Clear(ctx);
            pCurrentOutput->Bind(ctx);

            fx->Render(ctx, pCurrentInput);
            fullscreenQuad->Render(ctx);

            std::swap(pCurrentInput, pCurrentOutput);
        }
        return pCurrentInput; // 最後のswapの後、"最新の絵"はinput側に来ている
    }

private:
    std::vector<PostProcess*> m_effects;
};