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

class ModelResource {
public:
    ModelResource();
    ~ModelResource();

    // ファイルからモデルを読み込む（エントリーポイント）
    bool LoadFromFile(ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& filePath);

    // 外部のModelインスタンスが描画する際に参照するパーツリスト
    const std::vector<ModelPart>& GetParts() const { return m_parts; }

private:
    // Assimpのノード階層ツリーを再帰的に解析する関数
    void ProcessNode(struct aiNode* node, const struct aiScene* scene, ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX parentTransform);

    // 個々のメッシュ（頂点・インデックス）を解析して自前のMeshオブジェクトに変換する関数
    void ProcessMesh(struct aiMesh* mesh, const struct aiScene* scene, ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX transform);

private:
    std::vector<ModelPart> m_parts;

    // メモリ解放のために、このファイルが動的に生成した全アセットを追跡する
    std::vector<Mesh*> m_ownedMeshes;
    std::vector<Material*> m_ownedMaterials;
};