#include "scene4.h"
#include "normalMappingMaterial.h"
#include "model.h"
#include "mesh.h"
#include "shaderManager.h"

bool Scene4::Enter() {
    Mesh* g_pCubeMesh = nullptr;
    NormalMappingMaterial* g_pNormalMappingMaterial = nullptr;
    Model* g_pMainModel = nullptr;

    g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    g_pNormalMappingMaterial = new NormalMappingMaterial();
    g_pNormalMappingMaterial->CreateMaterialBuffer(m_device);
    if (!g_pNormalMappingMaterial->InitializeFromFile(m_device,
        ShaderManager::GetInstance().GetShader(ShaderID::NormalMapping),
        L"assets/nor/diff.png")) return false;
    if (!g_pNormalMappingMaterial->InitializeNormalMapFromFile(m_device,
        L"assets/nor/nor.png")) return false;

    g_pMainModel = new Model(m_device, g_pCubeMesh, g_pNormalMappingMaterial);
    g_pMainModel->SetPosition(0.0f, 0.0f, -4.0f);
    g_pMainModel->SetScale(2.0f, 2.0f, 2.0f);

    AddObject(std::make_unique<GameObject>(g_pMainModel, RenderPass::Opaque, BlendMode::Opaque));

    return true;
}