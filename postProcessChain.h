// PostProcessChain.h
#include<d3d11.h>
#include<vector>
#include"shaderManager.h"
#include"graphicsCommon.h"
#include"renderTarget.h"
#include"postProcess.h"

class PostProcessChain {
public:
    template<typename T>
    T* AddEffect(ID3D11Device* device, ShaderID shaderID, bool activeByDefault) {
        T* effect = new T();
        effect->Initialize(device, ShaderManager::GetInstance().GetShader(shaderID));
        effect->SetActive(activeByDefault);
        m_effects.push_back(effect);
        return effect; // Œã‚ÅŒÂ•Ê‚ÉG‚è‚½‚¢ê‡‚Ì‚½‚ß‚É•Ô‚µ‚Ä‚¨‚­
    }

    void Render(ID3D11DeviceContext* ctx, RenderTarget* source) {
        for (auto* fx : m_effects) {
            if (fx->IsActive()) fx->Render(ctx, source /* ÀÛ‚ÍpingpongŠÇ—‚ª•K—v */);
        }
    }

private:
    std::vector<PostProcess*> m_effects;
};