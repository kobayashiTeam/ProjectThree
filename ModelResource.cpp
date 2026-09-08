#include "modelResource.h"
#include "mesh.h"
#include "material.h"
#include "litMaterial.h"
#include "deferredCBMaterial.h"
#include "shaderManager.h"
#include "vertex.h"

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

bool ModelResource::LoadFromFile(ID3D11Device* pDevice, ShaderManager* pShaderManager,
    const std::wstring& filePath, ModelMaterialMode mode) {

    //追加：今回のロードで使うマテリアルの種類を記憶しておく
    // (ProcessMesh再帰の途中で参照するため、引数を増やさずメンバで持ち回す)
    m_materialMode = mode;

    Assimp::Importer importer;

    // Assimpはマルチバイト文字列を要求するため変換
    std::string pathStr(filePath.begin(), filePath.end());

    // LearnOpenGLからDirectXに置き換える際、最重要のフラグ
    unsigned int flags =
        aiProcess_Triangulate |             // ポリゴンを強制的に三角形にする
        aiProcess_ConvertToLeftHanded |     // 右手系からDirectX標準の「左手系」に一発変換
        aiProcess_CalcTangentSpace |        // 法線マップ用の接線（Tangent）を自動計算
        aiProcess_GenSmoothNormals;         // 法線がない場合に自動生成

    const aiScene* scene = importer.ReadFile(pathStr, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        OutputDebugStringA(importer.GetErrorString());
        return false;
    }

    // テクスチャを相対パスで読み込むために、ファイルの親ディレクトリを取得しておく
    // 例えば "Assets/Models/house.obj" なら "Assets/Models/" を抜き取る
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

    // このノードに含まれるメッシュ(パーツ)を処理
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, pDevice, pShaderManager, directory, globalTransform);
    }

    // 子ノードへ再帰
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, pDevice, pShaderManager, directory, globalTransform);
    }
}

void ModelResource::ProcessMesh(aiMesh* mesh, const aiScene* scene, ID3D11Device* pDevice,
    ShaderManager* pShaderManager, const std::wstring& directory, DirectX::XMMATRIX transform) {
    // SimpleVertex一本構造をやめて属性別に分ける
    std::vector<DirectX::XMFLOAT3> positions;
    std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT4> colors;
    std::vector<DirectX::XMFLOAT2> uvs;
    std::vector<DirectX::XMFLOAT3> tangents;
    std::vector<DWORD> indices;

    // --- 頂点データのコンバート ---
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {

        positions.push_back({
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
            });

        if (mesh->HasNormals()) {
            normals.push_back({
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
                });
        }
        else {
            normals.push_back({ 0.0f, 1.0f, 0.0f }); // フォールバック
        }

        colors.push_back({ 1.0f, 1.0f, 1.0f, 1.0f });

        if (mesh->mTextureCoords[0]) {
            uvs.push_back({
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
                });
        }
        else {
            uvs.push_back({ 0.0f, 0.0f }); // フォールバック
        }

        if (mesh->HasTangentsAndBitangents() && mesh->mTangents != nullptr) {
            tangents.push_back({
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z
                });
        }
        else {
            tangents.push_back({ 1.0f, 0.0f, 0.0f });
        }
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
        pDevice,
        positions.data(), normals.data(), colors.data(), uvs.data(), tangents.data(),
        (UINT)positions.size(),
        indices.data(), (UINT)indices.size()
    );

    m_ownedMeshes.push_back(newMesh); // 描画用メッシュとして登録

    // --- テクスチャパスの解決（Lit/Deferred共通） ---
    std::wstring fullTexPath;
    bool hasTexturePath = false;
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            std::string texPathSrc(texturePath.C_Str());
            std::wstring texPathW(texPathSrc.begin(), texPathSrc.end());
            fullTexPath = directory + texPathW;
            hasTexturePath = true;
        }
    }

    // --- マテリアル本連動(ここでLit/Deferredを分岐) ---
    Material* newMaterial = nullptr;

    if (m_materialMode == ModelMaterialMode::Deferred) {
        // ディファードGBufferパス用のマテリアルを生成
        Shader* pDeferredShader = ShaderManager::GetInstance().GetShader(ShaderID::DeferredGB);

        DeferredCBMaterial* deferredMat = new DeferredCBMaterial();
        bool loaded = false;
        if (hasTexturePath) {
            loaded = deferredMat->InitializeFromFile(pDevice, pDeferredShader, fullTexPath.c_str());
        }
        if (!loaded) {
            // テクスチャが無い/読み込めない場合はダミー白テクスチャにフォールバック
            UINT32 dummyPixels[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
            deferredMat->Initialize(pDevice, pDeferredShader, dummyPixels, 2, 2, true, false);
        }
        deferredMat->CreateMaterialBuffer(pDevice);
        deferredMat->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
        // デフォルト値。Scene側でモデル取得後に上書きする想定
        deferredMat->SetMetallic(0.0f);
        deferredMat->SetRoughness(0.5f);
        newMaterial = deferredMat;
    }
    else {
        // 従来通りのLitMaterial（フォワードパス用）
        Shader* pLitShader = ShaderManager::GetInstance().GetShader(ShaderID::Lit);

        LitMaterial* litMat = new LitMaterial();
        bool loaded = false;
        if (hasTexturePath) {
            loaded = litMat->InitializeFromFile(pDevice, pLitShader, fullTexPath.c_str());
        }
        if (!loaded) {
            UINT32 dummyPixels[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
            litMat->Initialize(pDevice, pLitShader, dummyPixels, 2, 2, true, true);
        }
        litMat->CreateMaterialBuffer(pDevice);
        litMat->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
        newMaterial = litMat;
    }

    m_ownedMaterials.push_back(newMaterial);

    // modelパーツとして登録
    ModelPart part;
    part.pMesh = newMesh;
    part.pMaterial = newMaterial;
    part.localTransform = transform; // Assimpから得たノードの配置行列
    m_parts.push_back(part);
}
