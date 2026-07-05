#pragma once
#include <d3d11.h>
#include <vector>
#include<DirectXMath.h>

//ユーザヘッダファイル
//#include "vertex.h"

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
        UINT vertexCount,
        const DWORD* indices,
        UINT indexCount);

    // パイプラインにバッファをセットして描画コマンドを発行する
    void Render(ID3D11DeviceContext* pImmediateContext);
    //test:instancing向けの描画
    void RenderInstanced(ID3D11DeviceContext* context,UINT instanceCount,
        ID3D11Buffer* pInstanceBuffer,UINT instanceStride);

    // 後片付け
    void Cleanup();

    // 静的ヘルパー関数
    static Mesh* CreateCube(ID3D11Device* pDevice, float size = 1.0f);
    static Mesh* CreatePlane(ID3D11Device* pDevice, float width = 10.0f, float depth = 10.0f);
    static Mesh* CreateQuad(ID3D11Device* pDevice, float size = 2.0f);

private:
    ID3D11Buffer* m_pVertexBuffer; // 頂点バッファ
    ID3D11Buffer* m_pIndexBuffer;  // インデックスバッファ
    UINT m_indexCount;             // インデックスの数（DrawIndexedで使用）

    //テスト：マルチストリーム
    // Multi-Stream後
    ID3D11Buffer* m_pPosBuffer=nullptr;
    ID3D11Buffer* m_pNrmBuffer = nullptr;
    ID3D11Buffer* m_pColorBuffer = nullptr;
    ID3D11Buffer* m_pUvBuffer = nullptr;
    //test:法線マップ用
	ID3D11Buffer* m_pTangentBuffer = nullptr;
};