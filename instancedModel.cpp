#include"instancedModel.h"
#include"material.h"
#include"mesh.h"

void InstancedModel::Render(ID3D11DeviceContext* context) {

    if (m_instanceData.empty() || !m_pInstanceBuffer) return;

    // ★GPUへの転送ロジック（Map/Unmap）がここに混ざる
    UINT count = (std::min)(
        static_cast<UINT>(m_instanceData.size()),
        m_maxInstances
        );
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    if (SUCCEEDED(context->Map(m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        memcpy(mappedResource.pData, m_instanceData.data(), sizeof(InstanceData) * count);
        context->Unmap(m_pInstanceBuffer, 0);
    }

    // 2. マテリアルバインド
    m_pInstMaterial->Bind(context);

    // 3. 描画（直接バッファのポインタとストライドのサイズを渡す）
    m_pMesh->RenderInstanced(context, count, m_pInstanceBuffer, sizeof(InstanceData));
}