#include "model.h"

Model::Model(Mesh* pMesh, Material* pMaterial)
    : m_pMesh(pMesh), m_pMaterial(pMaterial),
    m_Position(0.0f, 0.0f, 0.0f), m_Rotation(0.0f, 0.0f, 0.0f), m_Scale(1.0f, 1.0f, 1.0f)
{
}

Model::~Model()
{
    // メッシュとマテリアルは「共有アセット」の扱いにするため、
    // ここでは delete せず、管理元（main側など）に任せます。
}

DirectX::XMMATRIX Model::GetWorldMatrix() const
{
    // スケール * 回転 * 平行移動 (SRT) の順に行列を合成
    DirectX::XMMATRIX mScale = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    DirectX::XMMATRIX mRot = DirectX::XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    DirectX::XMMATRIX mTrans = DirectX::XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);

    return mScale * mRot * mTrans;
}

void Model::Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pConstantBuffer, Camera* pCamera, const ConstantBufferParameters& lightingParams)
{
    if (!m_pMesh || !m_pMaterial) return;

    m_pMaterial->Bind(pContext);

    ConstantBufferParameters cb;
    cb.mModel = DirectX::XMMatrixTranspose(GetWorldMatrix());
    cb.mView = DirectX::XMMatrixTranspose(pCamera->GetViewMatrix());
    cb.mProjection = DirectX::XMMatrixTranspose(pCamera->GetProjectionMatrix());
    cb.vLightPos = lightingParams.vLightPos;
    cb.vLightColor = lightingParams.vLightColor;
    cb.vEyePos = lightingParams.vEyePos;
    cb.vAttenuation = lightingParams.vAttenuation;

    pContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);

    // ↓ この2行が抜けていた
    pContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
    pContext->PSSetConstantBuffers(0, 1, &pConstantBuffer);

    m_pMesh->Render(pContext);
}