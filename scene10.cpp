#include "scene10.h"
#include <DirectXPackedVector.h>
#include "deferredCBMaterial.h"
#include "mesh.h"
#include "shaderManager.h"
#include "resourceManager.h"

bool Scene10::Enter() {

    // --- 土台（cube。metallic/roughnessは中間的な非金属で固定） ---
    Mesh* g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
    DeferredCBMaterial* g_pGroundMaterial = new DeferredCBMaterial();
    if (!g_pGroundMaterial->Initialize(m_device, ShaderManager::GetInstance().
        GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
    g_pGroundMaterial->CreateMaterialBuffer(m_device);
    g_pGroundMaterial->SetMaterialColor(0.5f, 0.5f, 0.5f, 1.0f);
    g_pGroundMaterial->SetMetallic(0.0f);
    g_pGroundMaterial->SetRoughness(0.8f);

    Model* g_pGroundModel = new Model(m_device, g_pCubeMesh, g_pGroundMaterial);
    g_pGroundModel->SetPosition(0.0f, -2.0f, 6.0f);
    g_pGroundModel->SetScale(6.0f, 0.2f, 6.0f);
    AddObject(std::make_unique<GameObject>(g_pGroundModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // --- 対照実験用のモデル本体（Scene9と同じgltfをキャッシュ経由で再利用） ---
    ModelResource* bagResource = ResourceManager::GetInstance().GetModel(
        m_device, L"assets/bag/scene.gltf", ModelMaterialMode::Deferred);

    if (!bagResource) {
        OutputDebugStringA("Scene10: failed to load bag model.\n");
        return false;
    }

    Model* bag = new Model(m_device, bagResource);

    // ★このシーン専用の複製を作る（Scene9側の個体や、リソース本体の値には影響させない）
    bag->CloneMaterialsForInstance(m_device);
    bag->SetMetallicRoughness(m_metallic, m_roughness); // 初期値を反映

    bag->SetPosition(0.0f, 0.0f, 6.0f);
    bag->SetScale(1.5f, 1.5f, 1.5f); // ★モデルの実寸に応じて調整してください

    // ★毎フレームUpdate()から値を書き換えられるように、生ポインタを控えておく
    // （GameObjectは所有権を持たない「借用」なので、二重解放の心配はない）
    m_targetModel = bag;

    AddObject(std::make_unique<GameObject>(bag, RenderPass::DeferredOpaque, BlendMode::Opaque));

    return true;
}
