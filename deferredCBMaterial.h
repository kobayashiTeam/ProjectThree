#pragma once
#include <DirectXMath.h> 
#include"material.h"

class DeferredCBMaterial : public Material
{
public:
    // このマテリアル専用の定数バッファ構造体（スロット2用）
    struct PerMaterialCB
    {
        DirectX::XMFLOAT4 vMaterialColor; // マテリアル固有の色
        float metallic;                   // 金属度 (0=非金属, 1=金属)
        float roughness;                  // 粗さ (0=つるつる, 1=ざらざら)
        DirectX::XMFLOAT2 padding;        // 16バイト境界に揃えるための埋め合わせ
    };

private:
    ID3D11Buffer* m_pMaterialBuffer = nullptr; // スロット2用バッファ
    PerMaterialCB m_cbData;                    // パラメータの生データ

public:
    DeferredCBMaterial() : m_pMaterialBuffer(nullptr) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(1, 1, 1, 1);
        m_cbData.metallic = 0.0f;
        m_cbData.roughness = 0.5f;
        m_cbData.padding = DirectX::XMFLOAT2(0, 0);
    }
    ~DeferredCBMaterial() override { if (m_pMaterialBuffer) m_pMaterialBuffer->Release(); }

    // バッファを生成するための初期化関数
    bool CreateMaterialBuffer(ID3D11Device* pDevice);

    // パラメータを変更するアクセサ（外部から色を変えられるようにする）
    void SetMaterialColor(float r, float g, float b, float a) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(r, g, b, a);
    }
    void SetMetallic(float m) { m_cbData.metallic = m; }
    void SetRoughness(float r) { m_cbData.roughness = r; }

    //親のBindを上書き（オーバーライド）して、自分専用のバッファもセットする
    void Bind(ID3D11DeviceContext* pContext) override;
};
