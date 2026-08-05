#pragma once
#include <d3d11.h>
#include "shader.h" 

class GSEffect;

class Material {
protected: // 派生クラスからアクセスできるように protected にする
    Shader* m_pShader;
    GSEffect* m_pGSEffect = nullptr;

    // マテリアルが使うテクスチャのSRVとサンプラー
    ID3D11ShaderResourceView* m_pTextureRV;
    ID3D11SamplerState* m_pSamplerLinear;

public:
    Material();
    virtual ~Material(); // 仮想デストラクタにしておく

    bool Initialize(ID3D11Device* pDevice, Shader* pShader,
        const void* pTexturePixels, UINT txtWidth, UINT txtHeight,bool isSRGB,bool isHDR);

    //virtual をつけて、派生クラスで拡張できるようにする
    virtual void Bind(ID3D11DeviceContext* pContext);
    bool InitializeFromFile(ID3D11Device* pDevice, Shader* pShader, const wchar_t* pFileName);
    void Cleanup();

    void SetGSEffect(GSEffect* gsEffect) {
        m_pGSEffect = gsEffect;
    }
};