#ifndef MATERIAL_H
#define MATERIAL_H

#include <d3d11.h>
#include <d3dcompiler.h>

class Material
{
public:
    Material();
    ~Material();

    // 初期化：シェーダーファイルパスやテクスチャの初期データを渡す
    bool Initialize(ID3D11Device* pDevice,
        const wchar_t* vsFileName,
        const wchar_t* psFileName,
        const UINT32* pTexturePixels, // 2x2のテクスチャデータ用
        UINT txtWidth, UINT txtHeight);

    // 後片付け
    void Cleanup();

    // パイプラインにこのマテリアルの状態をセットする
    void Bind(ID3D11DeviceContext* pContext);

private:
    ID3D11VertexShader* m_pVertexShader;
    ID3D11PixelShader* m_pPixelShader;
    ID3D11InputLayout* m_pVertexLayout;
    ID3D11ShaderResourceView* m_pTextureRV;
    ID3D11SamplerState* m_pSamplerLinear;
};

#endif // MATERIAL_H