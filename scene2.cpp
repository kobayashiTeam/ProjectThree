#include"scene2.h"
#include"litMaterial.h"
#include"mesh.h"
#include"shaderManager.h"

bool Scene2::Enter() {

    //宣言
    Mesh* g_pCubeMesh = nullptr;
    LitMaterial* g_pLitMaterial = nullptr;
    Model* g_pMainModel = nullptr; 

    // Mesh作成（Cube）
    g_pCubeMesh = Mesh::CreateCube(m_device, 1);

    // チェッカーテクスチャを手作り（4x4のモノクロ市松模様）
    // ガンマ補正の効果がわかりやすいように、白と暗いグレーのコントラストにしている
    const UINT texSize = 4;
    UINT32 checker[texSize * texSize];
    for (UINT y = 0; y < texSize; y++) {
        for (UINT x = 0; x < texSize; x++) {
            checker[y * texSize + x] = ((x + y) % 2 == 0) ? 0xFFFFFFFF : 0xFF202020;
        }
    }

    // LitMaterial：通常のカラーテクスチャとして sRGB フォーマットで生成
    // （isSRGB = true：ガンマ空間のテクスチャとしてサンプリング時にリニア化される）
    g_pLitMaterial = new LitMaterial();
    if (!g_pLitMaterial->Initialize(m_device, ShaderManager::GetInstance().
        GetShader(ShaderID::Lit), checker, texSize, texSize, true, false)) return false;
    g_pLitMaterial->CreateMaterialBuffer(m_device);
    g_pLitMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Model
    g_pMainModel = new Model(m_device, g_pCubeMesh, g_pLitMaterial);
    g_pMainModel->SetScale(2, 2, 2);
    g_pMainModel->SetPosition(0.0f, -0.5f, 3.0f);
    g_pMainModel->SetRotation(0.5f, 0.5f, 0.0f);

    AddObject(std::make_unique<GameObject>(g_pMainModel, RenderPass::Opaque, BlendMode::Opaque));

    return true;
}
