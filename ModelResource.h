#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

class Mesh;
class Material;
class ShaderManager;

// 3Dモデルを構成する「1つのパーツ」を管理する構造体
struct ModelPart {
    Mesh* pMesh = nullptr;
    Material* pMaterial = nullptr;
    DirectX::XMMATRIX localTransform; // そのパーツ固有の初期オフセット行列
};

//追加：読み込んだモデルをどちらのレンダリングパイプライン用マテリアルにするか
enum class ModelMaterialMode {
    Lit,        // 従来のフォワードLitMaterial（デフォルト・互換維持）
    Deferred    // DeferredCBMaterial（G-Bufferパス用）
};

class ModelResource {
public:
    ModelResource();
    ~ModelResource();

    // ファイルからモデルを読み込む（エントリーポイント）
    //変更：第4引数でLit/Deferredを選択可能に（省略時はLitのまま＝既存呼び出し箇所は無修正で動く）
    bool LoadFromFile(ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& filePath,
        ModelMaterialMode mode = ModelMaterialMode::Lit);

    // 描画時に参照するモデルパーツ一覧
    const std::vector<ModelPart>& GetParts() const { return m_parts; }

private:
    // Assimpのノード階層ツリーを再帰的に解析する関数
    void ProcessNode(struct aiNode* node, const struct aiScene* scene, ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX parentTransform);

    // 個々のメッシュ（頂点・インデックス）を解析して自前のMeshオブジェクトに変換する関数
    void ProcessMesh(struct aiMesh* mesh, const struct aiScene* scene, ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX transform);

private:
    std::vector<ModelPart> m_parts;

    // このリソースが所有する生成済みアセットを保持
    std::vector<Mesh*> m_ownedMeshes;
    std::vector<Material*> m_ownedMaterials;

    //追加：ProcessMesh実行中に参照する「今回はどっちのマテリアルで作るか」
    ModelMaterialMode m_materialMode = ModelMaterialMode::Lit;
};
