#include "scene6.h"
#include <DirectXPackedVector.h>
#include "deferredCBMaterial.h"
#include "mesh.h"
#include "shaderManager.h"

bool Scene6::Enter() {

    Mesh* g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    // ===== 浮いているcube（Bloomの主役：PointLightを近づけて輝度1.0超えを狙う）=====
    // DeferredOpaqueパスに登録することで、DeferredLightingShaderのBright(輝度1.0超え)判定を通す
    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
    DeferredCBMaterial* g_pMainMaterial = new DeferredCBMaterial();
    if (!g_pMainMaterial->Initialize(m_device, ShaderManager::
        GetInstance().GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pMainMaterial->CreateMaterialBuffer(m_device);
    g_pMainMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);

    Model* g_pMainModel = new Model(m_device, g_pCubeMesh, g_pMainMaterial);
    g_pMainModel->SetPosition(0.0f, 1.0f, 0.0f);
    g_pMainModel->SetScale(1.5f, 1.5f, 1.5f);

    AddObject(std::make_unique<GameObject>(g_pMainModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // ===== 床（同じcubeメッシュを大きく潰して床代わりにする）=====
    DeferredCBMaterial* g_pFloorMaterial = new DeferredCBMaterial();
    if (!g_pFloorMaterial->Initialize(m_device, ShaderManager::
        GetInstance().GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pFloorMaterial->CreateMaterialBuffer(m_device);
    g_pFloorMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);

    Model* g_pFloorModel = new Model(m_device, g_pCubeMesh, g_pFloorMaterial);
    g_pFloorModel->SetPosition(0.0f, -2.0f, 0.0f);
    g_pFloorModel->SetScale(8.0f, 0.5f, 8.0f);

    AddObject(std::make_unique<GameObject>(g_pFloorModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    return true;
}
