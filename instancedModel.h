#pragma once
#include "model.h"
#include <vector>
#include <algorithm>

struct InstanceData {
    DirectX::XMFLOAT4X4 WorldMatrix;
};

class InstancedModel : Model {
private:
    Mesh* m_pMesh = nullptr;
    Material* m_pInstMaterial = nullptr;
    std::vector<InstanceData> m_instanceData;

    // ★別クラスにせず、直接D3D11バッファのポインタを持つ
    ID3D11Buffer* m_pInstanceBuffer = nullptr;
    UINT m_maxInstances = 0;

public:
    ~InstancedModel() {
        // 解放処理をここに書く必要が出てくる
        if (m_pInstanceBuffer) {
            m_pInstanceBuffer->Release();
            m_pInstanceBuffer = nullptr;
        }
    }

    void Init(ID3D11Device* device, Mesh* mesh, Material* instMaterial, UINT maxInstances) {
        m_pMesh = mesh;
        m_pInstMaterial = instMaterial;
        m_maxInstances = maxInstances;

        // ★バッファの生成ロジックがここに混ざる
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.ByteWidth = sizeof(InstanceData) * m_maxInstances;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&desc, nullptr, &m_pInstanceBuffer);
    }

    void Render(ID3D11DeviceContext* context);
};