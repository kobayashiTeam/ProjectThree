#pragma once
#include <vector>
#include <algorithm>
#include <DirectXMath.h>
#include<d3d11.h>

class Mesh;


class InstancedModel{

public:
    struct InstanceData {
        DirectX::XMFLOAT4X4 WorldMatrix;
    };

    struct PerMaterialCB
    {
        DirectX::XMFLOAT4 vMaterialColor;
    };

private:
    Mesh* m_pMesh = nullptr;
    //Material* m_pInstMaterial = nullptr;
    std::vector<InstanceData> m_instanceData;

    ID3D11Buffer* m_pInstanceBuffer = nullptr;
    UINT m_maxInstances = 0;

    //自前でシェーダ関連オブジェクト
    ID3D11VertexShader* m_pVertexShader=nullptr;
    ID3D11PixelShader* m_pPixelShader = nullptr;
    ID3D11InputLayout* m_pVertexLayout = nullptr;
    //textrue
    ID3D11ShaderResourceView* m_pTextureRV = nullptr;
    ID3D11SamplerState* m_pSamplerLinear = nullptr;
    //material in shader
    ID3D11Buffer* m_pMaterialBuffer = nullptr;
    PerMaterialCB m_cbData;

public:
    ~InstancedModel() {
        if (m_pInstanceBuffer) {
            m_pInstanceBuffer->Release();
            m_pInstanceBuffer = nullptr;
        }
    }

    bool Init(ID3D11Device* pDevice, Mesh* mesh,UINT maxInstances);

    // ★追加メソッド①：データを全部リセットする（毎フレーム再構築する場合に使う）
    void ClearInstances() {
        m_instanceData.clear();
    }

    // ★追加メソッド②：インスタンスを1個追加する
    void AddInstance(const DirectX::XMFLOAT4X4& worldMatrix) {
        if (m_instanceData.size() >= m_maxInstances) return; // 上限ガード

        InstanceData data;
        data.WorldMatrix = worldMatrix;
        m_instanceData.push_back(data);
    }

    // パラメータを変更するアクセサ（外部から色を変えられるようにする）
    void SetMaterialColor(float r, float g, float b, float a) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(r, g, b, a);
    }

    void Render(ID3D11DeviceContext* context);
};