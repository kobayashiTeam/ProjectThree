#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "mesh.h"
#include "material.h"
#include "Camera.h"
#include "graphicsCommon.h"

class Model
{
public:
    // ★構造体の定義を「Modelクラスの内部」に引っ越し！
    // これにより、この構造体の正式名称は「Model::PerObjectCB」になり、外部と絶対衝突しなくなります。
    struct PerObjectCB
    {
        DirectX::XMMATRIX mModel;
    };

private:
    Mesh* m_pMesh;
    Material* m_pMaterial;

    DirectX::XMFLOAT3 m_Position;
    DirectX::XMFLOAT3 m_Rotation;
    DirectX::XMFLOAT3 m_Scale;

    ID3D11Buffer* m_pObjectBuffer = nullptr;

public:
    Model(ID3D11Device* pDevice, Mesh* pMesh, Material* pMaterial);
    ~Model();

    void SetPosition(float x, float y, float z) { m_Position = DirectX::XMFLOAT3(x, y, z); }
    void SetRotation(float x, float y, float z) { m_Rotation = DirectX::XMFLOAT3(x, y, z); }
    void SetScale(float x, float y, float z) { m_Scale = DirectX::XMFLOAT3(x, y, z); }

    const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
    DirectX::XMMATRIX GetWorldMatrix() const;

    void Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* pFrameBuffer);
};