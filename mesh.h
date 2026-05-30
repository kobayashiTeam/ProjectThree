#pragma once
#include <d3d11.h>
#include <vector>

//ユーザヘッダファイル
#include "vertex.h"

class Mesh
{
public:
    Mesh();
    ~Mesh();

    // 頂点データとインデックスデータからバッファを生成する
    bool Create(ID3D11Device* pDevice, const SimpleVertex* vertices, UINT vertexCount, const DWORD* indices, UINT indexCount);

    // パイプラインにバッファをセットして描画コマンドを発行する
    void Render(ID3D11DeviceContext* pImmediateContext);

    // 後片付け
    void Cleanup();

private:
    ID3D11Buffer* m_pVertexBuffer; // 頂点バッファ
    ID3D11Buffer* m_pIndexBuffer;  // インデックスバッファ
    UINT m_indexCount;             // インデックスの数（DrawIndexedで使用）
};