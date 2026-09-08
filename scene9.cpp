#include "scene9.h"
#include <DirectXPackedVector.h>
#include "deferredCBMaterial.h"
#include "mesh.h"
#include "shaderManager.h"
#include "resourceManager.h"

bool Scene9::Enter() {

    // --- 土台（cube、metallicもroughnessも中間的な値の非金属として固定） ---
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
    g_pGroundModel->SetPosition(4.0f, -4.0f, 6.0f);
    g_pGroundModel->SetScale(20.0f, 0.2f, 16.0f);
    AddObject(std::make_unique<GameObject>(g_pGroundModel, RenderPass::DeferredOpaque, BlendMode::Opaque));

    // --- ボール素材の読み込み（Deferredモード） ---
    //  パスは実際に配置したボール素材のファイル名に合わせて書き換えてください
    ModelResource* ballResource = ResourceManager::GetInstance().GetModel(
        m_device, L"assets/bag/scene.gltf", ModelMaterialMode::Deferred);

    if (!ballResource) {
        OutputDebugStringA("Scene9: failed to load ball model.\n");
        return false;
    }

    // --- グリッド配置：横=metallic、縦=roughness ---
    const float metallicValues[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    const float roughnessValues[] = { 0.1f, 0.4f, 0.7f };

    const float spacingX = 2.2f;
    const float spacingY = 2.2f;
    const float baseZ = 2.0f;

    // グリッド全体を画面中央に寄せるためのオフセット計算
    const float gridWidth = (5 - 1) * spacingX;
    const float gridHeight = (3 - 1) * spacingY;
    const float startX = -gridWidth * 0.5f;
    const float startY = gridHeight * 0.5f;

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 5; col++) {

            Model* ball = new Model(m_device, ballResource);

            //  このModelインスタンス専用のマテリアル複製を作る
            // （呼ばないと全部のボールが同じmetallic/roughnessで連動してしまう）
            ball->CloneMaterialsForInstance(m_device);
            ball->SetMetallicRoughness(metallicValues[col], roughnessValues[row]);

            ball->SetPosition(startX + col * spacingX, startY - row * spacingY, baseZ);
            ball->SetScale(1.0f, 1.0f, 1.0f); //  モデルの実寸に応じて調整してください

            AddObject(std::make_unique<GameObject>(ball, RenderPass::DeferredOpaque, BlendMode::Opaque));
        }
    }

    return true;
}
