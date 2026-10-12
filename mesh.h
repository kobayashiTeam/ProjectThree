#pragma once
#include <d3d11.h>
#include <vector>
#include<DirectXMath.h>
#include<DirectXCollision.h>


class Mesh
{
public:
    Mesh();
    ~Mesh();

    // 頂点データとインデックスデータからバッファを生成する
    bool Create(
        ID3D11Device* pDevice,
        const DirectX::XMFLOAT3* positions,
        const DirectX::XMFLOAT3* normals,
        const DirectX::XMFLOAT4* colors,
        const DirectX::XMFLOAT2* uvs,
        const DirectX::XMFLOAT3* tans,
        UINT vertexCount,
        const DWORD* indices,
        UINT indexCount);

    // パイプラインにバッファをセットして描画コマンドを発行する
    void Render(ID3D11DeviceContext* pImmediateContext);
    void RenderInstanced(ID3D11DeviceContext* context, UINT instanceCount,
        ID3D11Buffer* pInstanceBuffer, UINT instanceStride);

    void Cleanup();

    // ⑤視錐台カリング用：頂点位置から求めた、このメッシュのローカル空間でのAABB
    // （Create()の時点で一度だけ計算する。GPUへ送った後は頂点をCPU側に持たないため）
    const DirectX::BoundingBox& GetLocalBounds() const { return m_localBounds; }

    // 静的ヘルパー関数
    static Mesh* CreateCube(ID3D11Device* pDevice, float size = 1.0f);
    static Mesh* CreatePlane(ID3D11Device* pDevice, float width = 10.0f, float depth = 10.0f);
    static Mesh* CreateQuad(ID3D11Device* pDevice, float size = 2.0f);

private:
    ID3D11Buffer* m_pVertexBuffer; // 頂点バッファ
    ID3D11Buffer* m_pIndexBuffer;  // インデックスバッファ
    UINT m_indexCount;             // 描画するインデックス数

    ID3D11Buffer* m_pPosBuffer = nullptr;
    ID3D11Buffer* m_pNrmBuffer = nullptr;
    ID3D11Buffer* m_pColorBuffer = nullptr;
    ID3D11Buffer* m_pUvBuffer = nullptr;
    ID3D11Buffer* m_pTangentBuffer = nullptr;

    DirectX::BoundingBox m_localBounds; // 既定は中心(0,0,0)・半径(1,1,1)の箱
};