// material.h (修正版のイメージ)
#pragma once
#include <d3d11.h>
#include "shader.h" // 追加

class Material {
private:
    Shader* m_pShader = nullptr; // ★シェーダーへの参照（自分では解放しない）
    ID3D11ShaderResourceView* m_pTextureRV = nullptr;
    ID3D11SamplerState* m_pSamplerLinear = nullptr;

public:
    Material();
    ~Material();

    // 引数で Shader* を受け取る形に変更
    bool Initialize(ID3D11Device* pDevice, Shader* pShader, const UINT32* pTexturePixels, UINT txtWidth, UINT txtHeight);
    void Bind(ID3D11DeviceContext* pContext);
    void Cleanup();
};