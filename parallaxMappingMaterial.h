#pragma once
#include <DirectXMath.h>
#include"material.h"

class ParallaxMappingMaterial : public Material
{
public:
    // このマテリアル専用の定数バッファ構造体（スロット2用）
    struct PerMaterialCB
    {
        DirectX::XMFLOAT4 vMaterialColor; // マテリアル固有の色
    };

private:
    // 法線マップ・視差マップ用のSRVとサンプラー
    ID3D11ShaderResourceView* m_pNormalHeightMapTextureRV = nullptr;
    ID3D11SamplerState* m_pParallaxMapSamplerLinear = nullptr;
    ID3D11Buffer* m_pMaterialBuffer = nullptr; // スロット2用バッファ
    PerMaterialCB m_cbData;                    // パラメータの生データ

public:
    ParallaxMappingMaterial() {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(1, 1, 1, 1);
    }
    ~ParallaxMappingMaterial() override { if (m_pMaterialBuffer) m_pMaterialBuffer->Release(); }

    // バッファを生成するための初期化関数
    bool CreateMaterialBuffer(ID3D11Device* pDevice);

    // パラメータを変更するアクセサ（外部から色を変えられるようにする）
    void SetMaterialColor(float r, float g, float b, float a) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(r, g, b, a);
    }

    // 親のBindを上書き（オーバーライド）して、自分専用のバッファもセットする
    void Bind(ID3D11DeviceContext* pContext) override;

    bool InitializeParallaxMapFromFile(ID3D11Device* pDevice, const wchar_t* pFileName);
};