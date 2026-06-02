// material.h (修正版のイメージ)
#pragma once
#include <d3d11.h>
#include "shader.h" // 追加

class Material {
protected: // 派生クラスからアクセスできるように protected にする
    Shader* m_pShader;
    ID3D11ShaderResourceView* m_pTextureRV;
    ID3D11SamplerState* m_pSamplerLinear;

public:
    Material();
    virtual ~Material(); // 仮想デストラクタにしておく

    bool Initialize(ID3D11Device* pDevice, Shader* pShader,
        const UINT32* pTexturePixels, UINT txtWidth, UINT txtHeight);

    // ★ virtual をつけて、派生クラスで拡張できるようにする！
    virtual void Bind(ID3D11DeviceContext* pContext);
    void Cleanup();
};