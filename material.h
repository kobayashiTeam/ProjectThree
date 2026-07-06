// material.h (修正版のイメージ)
#pragma once
#include <d3d11.h>
#include "shader.h" // 追加

//前方
class GSEffect;

class Material {
protected: // 派生クラスからアクセスできるように protected にする
    Shader* m_pShader;
    //test:GS
    GSEffect* m_pGSEffect = nullptr;

    //マテリアルに使うテクスチャ、の設定を持ったview、とサンプラー
    ID3D11ShaderResourceView* m_pTextureRV;
    ID3D11SamplerState* m_pSamplerLinear;

public:
    Material();
    virtual ~Material(); // 仮想デストラクタにしておく

    bool Initialize(ID3D11Device* pDevice, Shader* pShader,
        const void* pTexturePixels, UINT txtWidth, UINT txtHeight,bool isSRGB,bool isHDR);

    // ★ virtual をつけて、派生クラスで拡張できるようにする！
    virtual void Bind(ID3D11DeviceContext* pContext);
    bool InitializeFromFile(ID3D11Device* pDevice, Shader* pShader, const wchar_t* pFileName);
    void Cleanup();

    //test:GS
    void SetGSEffect(GSEffect* gsEffect) {
        m_pGSEffect = gsEffect;
    }
};