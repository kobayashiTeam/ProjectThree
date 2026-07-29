#include "scene5.h"
#include "normalMappingMaterial.h"
#include "model.h"
#include "mesh.h"
#include "shaderManager.h"

bool Scene5::Enter() {
    Mesh* g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    // ===== 浮いているcube（影を落とす側）=====
    NormalMappingMaterial* g_pFloatingMaterial = new NormalMappingMaterial();
    g_pFloatingMaterial->CreateMaterialBuffer(m_device);
    if (!g_pFloatingMaterial->InitializeFromFile(m_device,
        ShaderManager::GetInstance().GetShader(ShaderID::NormalMapping),
        L"assets/nor/diff.png")) return false;
    if (!g_pFloatingMaterial->InitializeNormalMapFromFile(m_device,
        L"assets/nor/nor.png")) return false;

    Model* g_pFloatingModel = new Model(m_device, g_pCubeMesh, g_pFloatingMaterial);
    g_pFloatingModel->SetPosition(0.0f, 1.0f, 0.0f);
    g_pFloatingModel->SetScale(1.5f, 1.5f, 1.5f);

    AddObject(std::make_unique<GameObject>(g_pFloatingModel, RenderPass::Opaque, BlendMode::Opaque));

    // ===== 床（影を受け取る側：同じcubeメッシュを大きく潰して床代わりにする）=====
    NormalMappingMaterial* g_pFloorMaterial = new NormalMappingMaterial();
    g_pFloorMaterial->CreateMaterialBuffer(m_device);
    if (!g_pFloorMaterial->InitializeFromFile(m_device,
        ShaderManager::GetInstance().GetShader(ShaderID::NormalMapping),
        L"assets/nor/diff.png")) return false;
    if (!g_pFloorMaterial->InitializeNormalMapFromFile(m_device,
        L"assets/nor/nor.png")) return false;

    Model* g_pFloorModel = new Model(m_device, g_pCubeMesh, g_pFloorMaterial);
    g_pFloorModel->SetPosition(0.0f, -2.0f, 0.0f);
    g_pFloorModel->SetScale(8.0f, 0.5f, 8.0f);

    AddObject(std::make_unique<GameObject>(g_pFloorModel, RenderPass::Opaque, BlendMode::Opaque));

    return true;
}
