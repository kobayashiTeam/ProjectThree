#pragma once
#include<d3d11.h>
#include<DirectXMath.h>
#include <wrl/client.h>
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class Shader;

class PointSpriteGSEffect {

public:
    //独自cbの型をここで定義
    struct PerObjectCB//VS,b1
    {
        DirectX::XMMATRIX mModel;
    }m_cbModelData;

    struct PerSpriteCB
    {
        float SpriteSize; // 板ポリの一辺の半分のサイズ
        DirectX::XMFLOAT3 SpriteColor; // 色
    }m_cbSpriteData;

   bool Init(ID3D11Device* pDevice);        // シェーダー＋VB生成
    void Bind(ID3D11DeviceContext* pContext); // 全バインド
    void Draw(ID3D11DeviceContext* pContext); // トポロジー設定＋Draw

    // シェーダー
    Shader* m_pShader = nullptr;
    ID3D11GeometryShader* m_pGS = nullptr;

    // 頂点バッファ（点群）
    ComPtr<ID3D11Buffer> m_vb;
    UINT m_pointCount = 0;
    //cbバッファ
    ComPtr<ID3D11Buffer> m_cbModelb;
    ComPtr<ID3D11Buffer> m_cbSpriteb;

    //getter,setter
    void setGS(ID3D11GeometryShader* gs) {
        m_pGS = gs;
    }
    void setShader(Shader* shader) {
        m_pShader = shader;
    }
};
