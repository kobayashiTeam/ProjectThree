#include "model.h"
#include "mesh.h"
#include "material.h"

// 従来のコンストラクタ：単一のパーツとしてリストに1個だけ登録する（これで立方体も動く！）
Model::Model(ID3D11Device* pDevice, Mesh* pMesh, Material* pMaterial)
    : m_Position(0.0f, 0.0f, 0.0f), m_Rotation(0.0f, 0.0f, 0.0f), m_Scale(1.0f, 1.0f, 1.0f)
{
    ModelPart singlePart;
    singlePart.pMesh = pMesh;
    singlePart.pMaterial = pMaterial;
    //単位行列を書けるようなもの、変化なし。
    singlePart.localTransform = DirectX::XMMatrixIdentity(); // 立方体はオフセットなし
    m_Parts.push_back(singlePart);

    // 定数バッファ生成
    //モデルはmModel行列だけ持つ。描画の際にスロットに登録する。
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerObjectCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;
    pDevice->CreateBuffer(&cbd, nullptr, &m_pObjectBuffer);
}

// 新設コンストラクタ：ModelResourceが読み込んだパーツ群をまるごとコピーする
Model::Model(ID3D11Device* pDevice, const ModelResource* pResource)
    : m_Position(0.0f, 0.0f, 0.0f), m_Rotation(0.0f, 0.0f, 0.0f), m_Scale(1.0f, 1.0f, 1.0f)
{
    if (pResource)
    {
        m_Parts = pResource->GetParts(); // ベクターをまるごとコピー
    }

    // 定数バッファ生成
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerObjectCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;
    pDevice->CreateBuffer(&cbd, nullptr, &m_pObjectBuffer);
}

Model::~Model()
{
    if (m_pObjectBuffer)
    {
        m_pObjectBuffer->Release();
        m_pObjectBuffer = nullptr;
    }
}

DirectX::XMMATRIX Model::GetWorldMatrix() const
{
    DirectX::XMMATRIX mScale = 
        DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    DirectX::XMMATRIX mRot = 
        DirectX::XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    DirectX::XMMATRIX mTrans = 
        DirectX::XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    return mScale * mRot * mTrans;
}

void Model::Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pFrameBuffer)
{
    // フレームバッファ（スロット0）の適用はオブジェクト共通なのでループの前で1回
    pContext->VSSetConstantBuffers(0, 1, &pFrameBuffer);
    pContext->PSSetConstantBuffers(0, 1, &pFrameBuffer);

    // モデルが持つすべてのパーツをループ描画
    for (const auto& part : m_Parts)
    {
        if (!part.pMesh || !part.pMaterial) continue;

        // 1. マテリアルの適用
        part.pMaterial->Bind(pContext);

        // 2. ★超重要：このパーツ専用の行列を計算
        // 「パーツ自身のローカルオフセット」 × 「モデル全体の配置行列」
        DirectX::XMMATRIX finalWorld = DirectX::XMMatrixMultiply(part.localTransform, 
           GetWorldMatrix());

        //バッファは初期化時に生成されている。今はデータを作る
        Model::PerObjectCB objCB;
        objCB.mModel = DirectX::XMMatrixTranspose(finalWorld); // DirectX用に転置

        // 3. 定数バッファをパーツごとに書き換えてスロット1にバインド
        pContext->UpdateSubresource(m_pObjectBuffer, 0, nullptr, &objCB, 0, 0);
        pContext->VSSetConstantBuffers(1, 1, &m_pObjectBuffer);

        // 4. メッシュの描画
        part.pMesh->Render(pContext);
    }
}