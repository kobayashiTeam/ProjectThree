#pragma once
#include <DirectXMath.h> // ★これが必要
#include"material.h"

class NormalViz : public Material
{
public:
    // このマテリアル専用の定数バッファ構造体（スロット2用）
    struct PerMaterialCB
    {
        DirectX::XMFLOAT4 vMaterialColor; // マテリアル固有の色（今回はテスト用）
    };

private:
    ID3D11Buffer* m_pMaterialBuffer = nullptr; // スロット2用バッファ
    PerMaterialCB m_cbData;                    // パラメータの生データ

public:
    LitMaterial() : m_pMaterialBuffer(nullptr) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(1, 1, 1, 1);
    }
    ~LitMaterial() override { if (m_pMaterialBuffer) m_pMaterialBuffer->Release(); }

    // バッファを生成するための初期化関数
    bool CreateMaterialBuffer(ID3D11Device* pDevice);

    // パラメータを変更するアクセサ（外部から色を変えられるようにする）
    void SetMaterialColor(float r, float g, float b, float a) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(r, g, b, a);
    }

    // ★親のBindを上書き（オーバーライド）して、自分専用のバッファもセットする！
    void Bind(ID3D11DeviceContext* pContext) override;
};