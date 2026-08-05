#include"scene3.h"
#include <DirectXPackedVector.h>
#include"deferredCBMaterial.h"
#include"mesh.h"
#include"shaderManager.h"

bool Scene3::Enter() {

    //宣言
    Mesh* g_pCubeMesh = nullptr;
    DeferredCBMaterial* g_pDeferredCBMaterial = nullptr;
    Model* g_pMainModel = nullptr;
    Model* g_pGroundModel = nullptr; //土台cube（法線・深度の変化が分かりやすいように）

    // Mesh作成（Cube）
    g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    //DeferredCBMaterial（テクスチャ確認前段階なので白ベース）
    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
    g_pDeferredCBMaterial = new DeferredCBMaterial();
    if (!g_pDeferredCBMaterial->Initialize(m_device, ShaderManager::
        GetInstance().GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pDeferredCBMaterial->CreateMaterialBuffer(m_device);
    g_pDeferredCBMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Model
    g_pMainModel = new Model(m_device, g_pCubeMesh, g_pDeferredCBMaterial);
    g_pMainModel->SetScale(2, 2, 2);
    g_pMainModel->SetPosition(0.0f, -0.5f, 3.0f);
    g_pMainModel->SetRotation(0.5f, 0.5f, 0.0f);

    // Model：土台（法線の向き違いが見やすいように大きめに）
    g_pGroundModel = new Model(m_device, g_pCubeMesh, g_pDeferredCBMaterial);
    g_pGroundModel->SetPosition(0.0f, -6.2f, 5.0f);
    g_pGroundModel->SetScale(10.0f, 10.0f, 10.0f);

    AddObject(std::make_unique<GameObject>(g_pMainModel, RenderPass::DeferredOpaque, BlendMode::Opaque));
    AddObject(std::make_unique<GameObject>(g_pGroundModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    return true;
}
