#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "mesh.h"
#include "material.h"
#include "Camera.h"
#include "graphicsCommon.h"

class Model
{
private:
    Mesh* m_pMesh;       // 描画に使用するメッシュ（外部から貸与）
    Material* m_pMaterial;   // 描画に使用するマテリアル（外部から貸与）

    // 物体固有のトランスフォーム（位置、回転、拡大縮小）
    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Rotation; // 放射界（ラジアン）でのXYZ回転
    DirectX::XMFLOAT3 m_Scale;

public:
    Model(Mesh* pMesh, Material* pMaterial);
    ~Model();

    // アクセサ（位置や回転を外から操作できるようにする）
    void SetPosition(float x, float y, float z) { m_Position = DirectX::XMFLOAT3(x, y, z); }
    void SetRotation(float x, float y, float z) { m_Rotation = DirectX::XMFLOAT3(x, y, z); }
    void SetScale(float x, float y, float z) { m_Scale = DirectX::XMFLOAT3(x, y, z); }

    const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }

    // ワールド行列（Model行列）の計算
    DirectX::XMMATRIX GetWorldMatrix() const;

    // 描画処理（main.cpp にあった定数バッファ更新と描画の泥臭い部分をここに隠蔽）
    void Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pConstantBuffer, Camera* pCamera, const struct ConstantBufferParameters& lightingParams);
};