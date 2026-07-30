#include "scene8.h"
#include <DirectXPackedVector.h>

#include "deferredCBMaterial.h"
#include "normalMappingMaterial.h"
#include "mesh.h"
#include "model.h"
#include "shaderManager.h"

bool Scene8::Enter() {
    Mesh* g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    // ===== 手前・主役1：法線マッピングされた浮遊cube（影を落とす側） =====
    NormalMappingMaterial* g_pFloatingMaterial = new NormalMappingMaterial();
    g_pFloatingMaterial->CreateMaterialBuffer(m_device);
    if (!g_pFloatingMaterial->InitializeFromFile(m_device,
        ShaderManager::GetInstance().GetShader(ShaderID::NormalMapping),
        L"assets/nor/diff.png")) return false;
    if (!g_pFloatingMaterial->InitializeNormalMapFromFile(m_device,
        L"assets/nor/nor.png")) return false;

    Model* g_pFloatingModel = new Model(m_device, g_pCubeMesh, g_pFloatingMaterial);
    g_pFloatingModel->SetPosition(-2.0f, 1.0f, 0.0f);
    g_pFloatingModel->SetScale(1.5f, 1.5f, 1.5f);

    AddObject(std::make_unique<GameObject>(g_pFloatingModel, RenderPass::Opaque, BlendMode::Opaque));

    // ===== 手前・主役2：HDR輝度1.0超えのcube（PointLightを近づけるとBloom発火） =====
    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
    DeferredCBMaterial* g_pBloomMaterial = new DeferredCBMaterial();
    if (!g_pBloomMaterial->Initialize(m_device, ShaderManager::
        GetInstance().GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pBloomMaterial->CreateMaterialBuffer(m_device);
    g_pBloomMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);

    Model* g_pBloomModel = new Model(m_device, g_pCubeMesh, g_pBloomMaterial);
    g_pBloomModel->SetPosition(2.0f, 1.0f, 0.0f);
    g_pBloomModel->SetScale(1.5f, 1.5f, 1.5f);

    AddObject(std::make_unique<GameObject>(g_pBloomModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // ===== 床（影を受け取る側。奥のインスタンス群の足元まで届く大きさにする） =====
    NormalMappingMaterial* g_pFloorMaterial = new NormalMappingMaterial();
    g_pFloorMaterial->CreateMaterialBuffer(m_device);
    if (!g_pFloorMaterial->InitializeFromFile(m_device,
        ShaderManager::GetInstance().GetShader(ShaderID::NormalMapping),
        L"assets/nor/diff.png")) return false;
    if (!g_pFloorMaterial->InitializeNormalMapFromFile(m_device,
        L"assets/nor/nor.png")) return false;

    Model* g_pFloorModel = new Model(m_device, g_pCubeMesh, g_pFloorMaterial);
    g_pFloorModel->SetPosition(0.0f, -2.0f, -15.0f);
    g_pFloorModel->SetScale(20.0f, 0.5f, 40.0f);

    AddObject(std::make_unique<GameObject>(g_pFloorModel, RenderPass::Opaque, BlendMode::Opaque));

    return true;
}