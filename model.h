#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "graphicsCommon.h"
#include "ModelResource.h" 

class Mesh;
class Material;
class Shader;
class LitMaterial;

class Model
{
public:
    struct PerObjectCB
    {
        DirectX::XMMATRIX mModel;
    };

private:
    // 描画すべき「メッシュ＋マテリアルの組」を複数保持できるようにするためのリスト
    //model1つのなかに「メッシュ１つ、マテリアル１つの組」の集団が入るイメージ
    std::vector<ModelPart> m_Parts;

    // 追加：CloneMaterialsForInstance()で複製したマテリアルの所有権を持つリスト。
    // ModelResource由来のマテリアルはResource側が解放するが、複製した分はこのModel自身が
    // 解放責任を持つ必要があるため、別枠で保持しておく。
    std::vector<Material*> m_ownedClonedMaterials;

    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Rotation;
    DirectX::XMFLOAT3 m_Scale;

    ID3D11Buffer* m_pObjectBuffer = nullptr;
    bool m_isTransparent = false;

public:
    // 従来の「単一メッシュ用」コンストラクタ（立方体などの互換性を残すため）
    Model(ID3D11Device* pDevice, Mesh* pMesh, Material* pMaterial);
    // 派生クラス（ModelResourceから構築する場合など）向けのデフォルトコンストラクタ
    Model() : m_Position(0, 0, 0), m_Rotation(0, 0, 0), m_Scale(1, 1, 1), m_pObjectBuffer(nullptr) {}

    //「.gltfなどの外部リソース用」コンストラクタ
    Model(ID3D11Device* pDevice, const ModelResource* pResource);

    ~Model();

    void SetPosition(float x, float y, float z) { m_Position = DirectX::XMFLOAT3(x, y, z); }
    void SetRotation(float x, float y, float z) { m_Rotation = DirectX::XMFLOAT3(x, y, z); }
    void SetScale(float x, float y, float z) { m_Scale = DirectX::XMFLOAT3(x, y, z); }

    const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
    DirectX::XMMATRIX GetWorldMatrix() const;

    void Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pFrameBuffer);

    //transparent関連
    // 後から変更もできるようにしておく（演出用）
    void SetTransparent(bool enable) { m_isTransparent = enable; }
    bool IsTransparent() const { return m_isTransparent; }

    //shadowMap用
    void DrawGeometryOnly(ID3D11DeviceContext* ctx, ID3D11Buffer* pPerFrameCB);

    //追加：ModelResourceと共有しているマテリアルを、このModel専用の複製に置き換える。
    // 「同じgltfから複数のModelを作って、個別にmetallic/roughness等を変えたい」場合、
    // 一番最初に1回だけ呼ぶ想定。呼ばないと今まで通り全インスタンスで共有される。
    void CloneMaterialsForInstance(ID3D11Device* pDevice);

    // 追加：このModelが持つ全パーツに対してmetallic/roughnessをまとめて設定する。
    // 中身がDeferredCBMaterialでないパーツ（LitMaterial等）は黙って無視する。
    // 事前にCloneMaterialsForInstance()を呼んでおかないと、他のModelにも影響してしまうので注意。
    void SetMetallicRoughness(float metallic, float roughness);
};
