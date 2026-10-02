#include "scene11.h"
#include <DirectXPackedVector.h>
#include "deferredCBMaterial.h"
#include "model.h"
#include "mesh.h"
#include "shaderManager.h"

bool Scene11::Enter() {

    Mesh* g_pCubeMesh = Mesh::CreateCube(m_device, 1);
    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);

    // --- 床：カメラ(z=-10)の足元から奥へ約90m続く細長い板 ---
    DeferredCBMaterial* g_pGroundMaterial = new DeferredCBMaterial();
    if (!g_pGroundMaterial->Initialize(m_device, ShaderManager::GetInstance().
        GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pGroundMaterial->CreateMaterialBuffer(m_device);
    g_pGroundMaterial->SetMaterialColor(0.6f, 0.6f, 0.6f, 1.0f);
    g_pGroundMaterial->SetMetallic(0.0f);
    g_pGroundMaterial->SetRoughness(0.9f);

    Model* g_pGroundModel = new Model(m_device, g_pCubeMesh, g_pGroundMaterial);
    g_pGroundModel->SetPosition(0.0f, -2.0f, 35.0f);
    g_pGroundModel->SetScale(16.0f, 0.2f, 90.0f);
    AddObject(std::make_unique<GameObject>(g_pGroundModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // --- 柱の列：左右2列 × 手前から奥まで等間隔 ---
    // 手前・中間・遠くのどこにも同じ形の影が落ちるので、段ごとの精度差を見比べやすい
    DeferredCBMaterial* g_pPillarMaterial = new DeferredCBMaterial();
    if (!g_pPillarMaterial->Initialize(m_device, ShaderManager::GetInstance().
        GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pPillarMaterial->CreateMaterialBuffer(m_device);
    g_pPillarMaterial->SetMaterialColor(0.9f, 0.85f, 0.75f, 1.0f);
    g_pPillarMaterial->SetMetallic(0.0f);
    g_pPillarMaterial->SetRoughness(0.6f);

    const float pillarX[2] = { -3.5f, 3.5f };
    for (int side = 0; side < 2; side++)
    {
        for (float z = -4.0f; z <= 56.0f; z += 6.0f)
        {
            Model* pillar = new Model(m_device, g_pCubeMesh, g_pPillarMaterial);
            pillar->SetPosition(pillarX[side], -0.4f, z);
            pillar->SetScale(0.8f, 3.0f, 0.8f); // 下端が床の上面(y=-1.9)付近に来る高さ
            AddObject(std::make_unique<GameObject>(pillar, RenderPass::DeferredOpaque, BlendMode::Opaque));
        }
    }

    return true;
}
