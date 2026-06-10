#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include "graphicsCommon.h"
#include "ModelResource.h" // ★追加：パーツ情報の構造体(ModelPart)を参照するため

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
    //追加
    LitMaterial* m_pOverrideMaterial = nullptr;   // ← 新規追加

private:
    // ↓【変更】単一のポインタ保持から、描画すべきパーツのリスト保持に拡張
    //model1つのなかに「メッシュ１つ、マテリアル１つの組」の集団が入るイメージ
    std::vector<ModelPart> m_Parts;

    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Rotation;
    DirectX::XMFLOAT3 m_Scale;

    ID3D11Buffer* m_pObjectBuffer = nullptr;

public:
    // ★従来の「単一メッシュ用」コンストラクタ（立方体などの互換性を残すため）
    Model(ID3D11Device* pDevice, Mesh* pMesh, Material* pMaterial);

    // ★【新設】「.gltfなどの外部リソース用」コンストラクタ
    Model(ID3D11Device* pDevice, const ModelResource* pResource);

    ~Model();

    void SetPosition(float x, float y, float z) { m_Position = DirectX::XMFLOAT3(x, y, z); }
    void SetRotation(float x, float y, float z) { m_Rotation = DirectX::XMFLOAT3(x, y, z); }
    void SetScale(float x, float y, float z) { m_Scale = DirectX::XMFLOAT3(x, y, z); }

    const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
    DirectX::XMMATRIX GetWorldMatrix() const;

    void Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pFrameBuffer);

    //テスト
    // アウトライン描画用に一時的にPixel Shaderを差し替える
    void SetMaterialOverride(LitMaterial* pMat) { m_pOverrideMaterial = pMat; }

    // オーバーライドを解除
    void ResetMaterialOverride() { m_pOverrideMaterial = nullptr; }
};