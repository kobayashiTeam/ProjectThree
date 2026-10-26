#include "scene12.h"
#include <DirectXPackedVector.h>
#include "deferredCBMaterial.h"
#include "model.h"
#include "mesh.h"
#include "shaderManager.h"

bool Scene12::Enter() {

    Mesh* g_pCubeMesh = Mesh::CreateCube(m_device, 1);
    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);

    // --- 床：上面が y = -1.9（ParticleSystem の floorY と同じ高さ）。粒が跳ね返る ---
    DeferredCBMaterial* g_pGroundMaterial = new DeferredCBMaterial();
    if (!g_pGroundMaterial->Initialize(m_device, ShaderManager::GetInstance().
        GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pGroundMaterial->CreateMaterialBuffer(m_device);
    g_pGroundMaterial->SetMaterialColor(0.25f, 0.25f, 0.28f, 1.0f); // 暗めにして、光る粒を目立たせる
    g_pGroundMaterial->SetMetallic(0.0f);
    g_pGroundMaterial->SetRoughness(0.8f);

    Model* g_pGroundModel = new Model(m_device, g_pCubeMesh, g_pGroundMaterial);
    g_pGroundModel->SetPosition(0.0f, -2.0f, 4.0f);
    g_pGroundModel->SetScale(16.0f, 0.2f, 16.0f);
    AddObject(std::make_unique<GameObject>(g_pGroundModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // --- 噴水の台座：発生点（0, -1.9, 4）の真下に置く目印 ---
    DeferredCBMaterial* g_pBaseMaterial = new DeferredCBMaterial();
    if (!g_pBaseMaterial->Initialize(m_device, ShaderManager::GetInstance().
        GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pBaseMaterial->CreateMaterialBuffer(m_device);
    g_pBaseMaterial->SetMaterialColor(0.7f, 0.65f, 0.6f, 1.0f);
    g_pBaseMaterial->SetMetallic(0.0f);
    g_pBaseMaterial->SetRoughness(0.5f);

    Model* g_pBaseModel = new Model(m_device, g_pCubeMesh, g_pBaseMaterial);
    g_pBaseModel->SetPosition(0.0f, -1.85f, 4.0f);
    g_pBaseModel->SetScale(0.6f, 0.1f, 0.6f);
    AddObject(std::make_unique<GameObject>(g_pBaseModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // --- パーティクル：最大数（100万）で1回だけ作る ---
    m_particles = std::make_unique<ParticleSystem>();
    if (!m_particles->Initialize(m_device, COUNTS[COUNT_LEVELS - 1])) return false;
    m_particles->SetActiveCount(COUNTS[m_countIndex]);

    return true;
}
