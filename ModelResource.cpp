#include "modelResource.h"
#include "mesh.h"
#include "material.h"
#include "litMaterial.h" // 必要に応じて使い分け
#include "shaderManager.h"
#include "vertex.h"      // SimpleVertex が定義されているヘッダ

// Assimpのインクルード
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

ModelResource::ModelResource() {}

ModelResource::~ModelResource() {
    // 自身が生成したMeshとMaterialをすべて安全に解放
    for (auto* m : m_ownedMeshes) { delete m; }
    for (auto* mat : m_ownedMaterials) { delete mat; }
}

bool ModelResource::LoadFromFile(ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& filePath) {
    Assimp::Importer importer;

    // Assimpはマルチバイト文字列を要求するため変換
    std::string pathStr(filePath.begin(), filePath.end());

    // ★ LearnOpenGLからDirectXに置き換える際、最重要のフラグ
    unsigned int flags =
        aiProcess_Triangulate |             // ポリゴンを強制的に三角形にする
        aiProcess_ConvertToLeftHanded |     // ★超重要: 右手系からDirectX標準の「左手系」に一発変換
        aiProcess_CalcTangentSpace |        // 法線マップ用の接線（Tangent）を自動計算
        aiProcess_GenSmoothNormals;         // 法線がない場合に自動生成

    const aiScene* scene = importer.ReadFile(pathStr, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        OutputDebugStringA(importer.GetErrorString());
        return false;
    }

    // テクスチャを相対パスで読み込むために、ファイルの親ディレクトリを取得しておく
    std::wstring directory = filePath.substr(0, filePath.find_last_of(L"/\\") + 1);

    // 初期行列（単位行列）から再帰解析を開始
    DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
    ProcessNode(scene->mRootNode, scene, pDevice, pShaderManager, directory, identity);

    return true;
}

void ModelResource::ProcessNode(aiNode* node, const aiScene* scene, ID3D11Device* pDevice, 
    ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX parentTransform) {
    // Assimpの4x4行列を DirectXMath の XMMATRIX に変換
    aiMatrix4x4 m = node->mTransformation;
    DirectX::XMMATRIX localTransform = DirectX::XMMatrixSet(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
    // 親ノードのトランスフォームと掛け合わせる
    DirectX::XMMATRIX globalTransform = DirectX::XMMatrixMultiply(localTransform, parentTransform);

    // このノードに含まれるメッシュ（パーツ）を処理
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, pDevice, pShaderManager, directory, globalTransform);
    }

    // 子ノードへ再帰
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, pDevice, pShaderManager, directory, globalTransform);
    }
}

void ModelResource::ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D11Device* pDevice, ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX transform) {
    std::vector<SimpleVertex> vertices;
    std::vector<DWORD> indices;

    // --- 頂点データのコンバート ---
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        SimpleVertex vertex = {};

        // 座標 (X, Y, Z)
        vertex.x = mesh->mVertices[i].x;//Pos.x
        vertex.y = mesh->mVertices[i].y;
        vertex.z = mesh->mVertices[i].z;

        // 法線 (Normal)
        if (mesh->HasNormals()) {
            vertex.nx = mesh->mNormals[i].x;//Normak.x
            vertex.ny = mesh->mNormals[i].y;
            vertex.nz = mesh->mNormals[i].z;
        }

        // UV座標 (テクスチャ座標)
        if (mesh->mTextureCoords[0]) {
            vertex.u = mesh->mTextureCoords[0][i].x;//Tex.x
            vertex.v = mesh->mTextureCoords[0][i].y;
            // ※ aiProcess_ConvertToLeftHanded を指定していれば、V軸(Y)の反転（1.0f - y）はAssimpが自動でやってくれます！
        }

        // あなたの頂点構造体のカラー初期値などがあれば適宜設定
        //vertex.Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        vertex.r = 1.0f;
        vertex.g = 1.0f;
        vertex.b = 1.0f;

        vertices.push_back(vertex);
    }

    // --- インデックスデータのコンバート ---
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // 自前のMeshオブジェクトを生成
    Mesh* newMesh = new Mesh();
    newMesh->Create(
        pDevice, vertices.data(), (UINT)vertices.size(), indices.data(), (UINT)indices.size());

    m_ownedMeshes.push_back(newMesh);

    // --- マテリアル（テクスチャ）の本連動 ---
    Material* newMaterial = nullptr;
    Shader* pLitShader = pShaderManager->GetOrCreate(pDevice, L"LitShader.hlsl");

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {

            std::string texPathSrc(texturePath.C_Str());
            std::wstring texPathW(texPathSrc.begin(), texPathSrc.end());

            // 親ディレクトリパスと結合
            std::wstring fullTexPath = directory + texPathW;

            // ★【変更】増設した InitializeFromFile を呼び出す！
            LitMaterial* litMat = new LitMaterial();
            if (litMat->InitializeFromFile(pDevice, pLitShader, fullTexPath.c_str())) {
                litMat->CreateMaterialBuffer(pDevice);
                litMat->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
                newMaterial = litMat;
            }
            else {
                // 万が一画像のロードに失敗した場合は、安全のためにデリートしてフォールバックへ落とす
                delete litMat;
                litMat = nullptr;
            }
        }
    }

    // テクスチャがない、またはロード失敗時はチェッカー模様（既存の安全装置）
    if (!newMaterial) {
        UINT32 dummyPixels[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
        LitMaterial* litMat = new LitMaterial();
        litMat->Initialize(pDevice, pLitShader, dummyPixels, 2, 2);
        litMat->CreateMaterialBuffer(pDevice);
        litMat->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
        newMaterial = litMat;
    }

    m_ownedMaterials.push_back(newMaterial);

    // パーツとして登録
    ModelPart part;
    part.pMesh = newMesh;
    part.pMaterial = newMaterial;
    part.localTransform = transform; // Assimpから得たノードの配置行列
    m_parts.push_back(part);
}