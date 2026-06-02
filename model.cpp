#include "model.h"

Model::Model(ID3D11Device* pDevice, Mesh* pMesh, Material* pMaterial)
    : m_pMesh(pMesh), m_pMaterial(pMaterial),
    m_Position(0.0f, 0.0f, 0.0f), m_Rotation(0.0f, 0.0f, 0.0f), m_Scale(1.0f, 1.0f, 1.0f)
{
    // ★自分専用の定数バッファ（PerObjectCB用）を生成
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerObjectCB); // ワールド行列1枚分のサイズ
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    pDevice->CreateBuffer(&cbd, nullptr, &m_pObjectBuffer);
}

Model::~Model()
{
    // ★自分が生成した独占バッファなので、ここで責任を持って解放
    if (m_pObjectBuffer)
    {
        m_pObjectBuffer->Release();
        m_pObjectBuffer = nullptr;
    }
}

DirectX::XMMATRIX Model::GetWorldMatrix() const
{
    DirectX::XMMATRIX mScale = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    DirectX::XMMATRIX mRot = DirectX::XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    DirectX::XMMATRIX mTrans = DirectX::XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    return mScale * mRot * mTrans;
}

void Model::Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pFrameBuffer)
{
    if (!m_pMesh || !m_pMaterial) return;

    // 1. マテリアル（シェーダーやテクスチャ）を適用
    m_pMaterial->Bind(pContext);

    // 2. ★【スロット0】フレームバッファの適用（main側で既に中身が更新されている前提）
    // 引数で渡された pFrameBuffer をそのままスロット0にセット
    pContext->VSSetConstantBuffers(0, 1, &pFrameBuffer);
    pContext->PSSetConstantBuffers(0, 1, &pFrameBuffer);

    // 3. ★【スロット1】オブジェクトバッファ（自身専用）の更新と適用
    Model::PerObjectCB objCB;
    objCB.mModel = DirectX::XMMatrixTranspose(GetWorldMatrix()); // 自身の行列

    pContext->UpdateSubresource(m_pObjectBuffer, 0, nullptr, &objCB, 0, 0);
    pContext->VSSetConstantBuffers(1, 1, &m_pObjectBuffer); // スロット1にセット！
    // ※UnlitShaderのピクセルシェーダーではmModelを使わないので、今回はVS側だけでOKです

    // 4. メッシュの描画
    m_pMesh->Render(pContext);
}