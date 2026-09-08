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
        const void* pTexturePixels, UINT txtWidth, UINT txtHeight, bool isSRGB, bool isHDR);

    //virtual をつけて、派生クラスで拡張できるようにする
    virtual void Bind(ID3D11DeviceContext* pContext);
    bool InitializeFromFile(ID3D11Device* pDevice, Shader* pShader, const wchar_t* pFileName);
    void Cleanup();

    // 追加：自分の複製を作る（デフォルト実装）
    // テクスチャ・サンプラーはCOMの参照カウントを増やして共有し、シェーダーポインタも共有する。
    // 「個別に変えたいパラメータ」を持つ派生クラス（DeferredCBMaterial等）は
    // これをoverrideして、そのパラメータ用バッファだけ新規に作り直すこと。
    virtual Material* Clone(ID3D11Device* pDevice) const;

    void SetGSEffect(GSEffect* gsEffect) {
        m_pGSEffect = gsEffect;
    }
};
