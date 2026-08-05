#pragma once
#include <vector>
#include <algorithm>
#include <DirectXMath.h>
#include<d3d11.h>

class Mesh;


class InstancedModel{

public:
    struct InstanceData {
        DirectX::XMFLOAT4 row0;
        DirectX::XMFLOAT4 row1;
        DirectX::XMFLOAT4 row2;
        DirectX::XMFLOAT4 row3;
    };

    struct PerMaterialCB
    {
        DirectX::XMFLOAT4 vMaterialColor;
    };

private:
    Mesh* m_pMesh = nullptr;
    std::vector<InstanceData> m_instanceData;
    // Scene7用：maxInstances分の座標を最初に1回だけ計算しておくテーブル（オブジェクトプール的発想）
    std::vector<InstanceData> m_positionTable;

    ID3D11Buffer* m_pInstanceBuffer = nullptr;
    UINT m_maxInstances = 0;

    //自前でシェーダ関連オブジェクト
    ID3D11VertexShader* m_pVertexShader=nullptr;
    ID3D11PixelShader* m_pPixelShader = nullptr;
    ID3D11InputLayout* m_pVertexLayout = nullptr;
    //texture
    ID3D11ShaderResourceView* m_pTextureRV = nullptr;
    ID3D11SamplerState* m_pSamplerLinear = nullptr;
    //material in shader
    ID3D11Buffer* m_pMaterialBuffer = nullptr;
    PerMaterialCB m_cbData;

public:
    ~InstancedModel() {
        if (m_pInstanceBuffer) { m_pInstanceBuffer->Release(); m_pInstanceBuffer = nullptr; }
        if (m_pVertexShader) { m_pVertexShader->Release();   m_pVertexShader = nullptr; }
        if (m_pPixelShader) { m_pPixelShader->Release();    m_pPixelShader = nullptr; }
        if (m_pVertexLayout) { m_pVertexLayout->Release();   m_pVertexLayout = nullptr; }
        if (m_pTextureRV) { m_pTextureRV->Release();      m_pTextureRV = nullptr; }
        if (m_pSamplerLinear) { m_pSamplerLinear->Release();  m_pSamplerLinear = nullptr; }
        if (m_pMaterialBuffer) { m_pMaterialBuffer->Release(); m_pMaterialBuffer = nullptr; }
    }

    bool Init(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,Mesh* mesh,UINT maxInstances);

    // データを全部リセットする（毎フレーム再構築する場合に使う）
    void ClearInstances() {
        m_instanceData.clear();
    }

    // Scene7用：座標テーブルの先頭からcount個だけを有効化する（テーブル自体は再計算しない）
    void SetActiveCount(UINT count);

    // Scene8用：count個を有効化しつつ、群全体を指定オフセット分だけ平行移動する
    // （テーブル内の相対配置=spacingは変えず、群れごと任意のワールド座標へ動かす）
    void SetActiveCount(UINT count, const DirectX::XMFLOAT3& offset);

    // インスタンスを1個追加する
    void AddInstance(const InstanceData& data) {
        if (m_instanceData.size() >= m_maxInstances) return; // 上限ガード

        m_instanceData.push_back(data);
    }

    // パラメータを変更するアクセサ（外部から色を変えられるようにする）
    void SetMaterialColor(float r, float g, float b, float a) {
        m_cbData.vMaterialColor = DirectX::XMFLOAT4(r, g, b, a);
    }

    void Render(ID3D11DeviceContext* context);

    InstanceData MatrixToInstanceData(DirectX::XMMATRIX m)
    {
        DirectX::XMFLOAT4X4 f;
        XMStoreFloat4x4(&f, m);           // row-major で書き出し
        InstanceData d;
        d.row0 = { f._11, f._12, f._13, f._14 };
        d.row1 = { f._21, f._22, f._23, f._24 };
        d.row2 = { f._31, f._32, f._33, f._34 };
        d.row3 = { f._41, f._42, f._43, f._44 };
        return d;
    }
};