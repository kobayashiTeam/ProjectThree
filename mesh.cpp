#include "mesh.h"
#include"vertex.h"

Mesh::Mesh()
    : m_pVertexBuffer(nullptr)
    , m_pIndexBuffer(nullptr)
    , m_indexCount(0)
{
}

Mesh::~Mesh()
{
    Cleanup();
}

bool Mesh::Create(
    ID3D11Device* pDevice,
    const DirectX::XMFLOAT3* positions,
    const DirectX::XMFLOAT3* normals,
    const DirectX::XMFLOAT4* colors,
    const DirectX::XMFLOAT2* uvs,
    const DirectX::XMFLOAT3* tans,
    UINT vertexCount,
    const DWORD* indices,
    UINT indexCount)
{
    Cleanup();
    m_indexCount = indexCount;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};

    // Position
    bd.ByteWidth = sizeof(DirectX::XMFLOAT3) * vertexCount;
    initData.pSysMem = positions;
    if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_pPosBuffer)))   return false;

    // Normal
    bd.ByteWidth = sizeof(DirectX::XMFLOAT3) * vertexCount;
    initData.pSysMem = normals;
    if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_pNrmBuffer)))   return false;

    // Color
    bd.ByteWidth = sizeof(DirectX::XMFLOAT4) * vertexCount;
    initData.pSysMem = colors;
    if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_pColorBuffer))) return false;

    // UV
    bd.ByteWidth = sizeof(DirectX::XMFLOAT2) * vertexCount;
    initData.pSysMem = uvs;
    if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_pUvBuffer)))    return false;

    //Tangent
    if (tans != nullptr) {
        bd.ByteWidth = sizeof(DirectX::XMFLOAT3) * vertexCount;
        initData.pSysMem = tans;
        if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_pTangentBuffer))) return false;
    }
    else {
        //Tangentデータが渡されなかった場合は、ダミー（ゼロ）でバッファを作成
        std::vector<DirectX::XMFLOAT3> dummyTangents(vertexCount, DirectX::XMFLOAT3(0, 0, 0));
        bd.ByteWidth = sizeof(DirectX::XMFLOAT3) * vertexCount;
        initData.pSysMem = dummyTangents.data();
        if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_pTangentBuffer))) return false;
    }

    // Index
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(DWORD) * indexCount;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    initData.pSysMem = indices;
    if (FAILED(pDevice->CreateBuffer(&ibd, &initData, &m_pIndexBuffer))) return false;

    return true;
}

void Mesh::Render(ID3D11DeviceContext* pImmediateContext)
{
    if (!m_pPosBuffer || !m_pIndexBuffer) return;

    ID3D11Buffer* vbs[5] = { m_pPosBuffer, m_pNrmBuffer, m_pColorBuffer, m_pUvBuffer,m_pTangentBuffer };
    UINT strides[5] = { 
        sizeof(DirectX::XMFLOAT3), 
        sizeof(DirectX::XMFLOAT3), 
        sizeof(DirectX::XMFLOAT4), 
        sizeof(DirectX::XMFLOAT2),
        sizeof(DirectX::XMFLOAT3), };
    UINT offsets[5] = { 0, 0, 0, 0,0 };
    pImmediateContext->IASetVertexBuffers(0, 5, vbs, strides, offsets);
    pImmediateContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    pImmediateContext->DrawIndexed(m_indexCount, 0, 0);
}

void Mesh::Cleanup()
{
    if (m_pIndexBuffer) { m_pIndexBuffer->Release();  m_pIndexBuffer = nullptr; }
    if (m_pVertexBuffer) { m_pVertexBuffer->Release(); m_pVertexBuffer = nullptr; }
    m_indexCount = 0;
}

Mesh* Mesh::CreateCube(ID3D11Device* pDevice, float size) {

    DirectX::XMFLOAT3 positions[] =
    {
        {-0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},

        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},

        {-0.5f,  0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},

        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f,  0.5f},
        {-0.5f, -0.5f,  0.5f},

        {-0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},

        { 0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f, -0.5f}
    };

    DirectX::XMFLOAT3 normals[] =
    {
        { 0.0f,  0.0f, -1.0f},
        { 0.0f,  0.0f, -1.0f},
        { 0.0f,  0.0f, -1.0f},
        { 0.0f,  0.0f, -1.0f},

        { 0.0f,  0.0f,  1.0f},
        { 0.0f,  0.0f,  1.0f},
        { 0.0f,  0.0f,  1.0f},
        { 0.0f,  0.0f,  1.0f},

        { 0.0f,  1.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f},

        { 0.0f, -1.0f,  0.0f},
        { 0.0f, -1.0f,  0.0f},
        { 0.0f, -1.0f,  0.0f},
        { 0.0f, -1.0f,  0.0f},

        {-1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},

        { 1.0f,  0.0f,  0.0f},
        { 1.0f,  0.0f,  0.0f},
        { 1.0f,  0.0f,  0.0f},
        { 1.0f,  0.0f,  0.0f}
    };

    DirectX::XMFLOAT4 colors[24] =
    {
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},

        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},

        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},

        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},

        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},

        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f},
        {1.0f,1.0f,1.0f,1.0f}
    };

    DirectX::XMFLOAT2 uvs[] =
    {
        {0.0f,0.0f},
        {1.0f,0.0f},
        {1.0f,1.0f},
        {0.0f,1.0f},

        {0.0f,0.0f},
        {1.0f,0.0f},
        {1.0f,1.0f},
        {0.0f,1.0f},

        {0.0f,0.0f},
        {1.0f,0.0f},
        {1.0f,1.0f},
        {0.0f,1.0f},

        {0.0f,0.0f},
        {1.0f,0.0f},
        {1.0f,1.0f},
        {0.0f,1.0f},

        {0.0f,0.0f},
        {1.0f,0.0f},
        {1.0f,1.0f},
        {0.0f,1.0f},

        {0.0f,0.0f},
        {1.0f,0.0f},
        {1.0f,1.0f},
        {0.0f,1.0f}
    };


    DirectX::XMFLOAT3 tangents[] =
    {
        // 1. 手前面 (Normal: 0, 0, -1) -> UVの右方向は「空間の右(+X)」
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },

        // 2. 奥面 (Normal: 0, 0, 1) -> UVの右方向は「空間の左(-X)」
        {-1.0f,  0.0f,  0.0f },
        {-1.0f,  0.0f,  0.0f },
        {-1.0f,  0.0f,  0.0f },
        {-1.0f,  0.0f,  0.0f },

        // 3. 上面 (Normal: 0, 1, 0) -> UVの右方向は「空間の右(+X)」
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },

        // 4. 底面 (Normal: 0, -1, 0) -> UVの右方向は「空間の右(+X)」
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },
        { 1.0f,  0.0f,  0.0f },

        // 5. 左側面 (Normal: -1, 0, 0) -> UVの右方向は「空間の奥(-Z)」
        { 0.0f,  0.0f, -1.0f },
        { 0.0f,  0.0f, -1.0f },
        { 0.0f,  0.0f, -1.0f },
        { 0.0f,  0.0f, -1.0f },

        // 6. 右側面 (Normal: 1, 0, 0) -> UVの右方向は「空間の手前(+Z)」
        { 0.0f,  0.0f,  1.0f },
        { 0.0f,  0.0f,  1.0f },
        { 0.0f,  0.0f,  1.0f },
        { 0.0f,  0.0f,  1.0f }
    };


    DWORD indices[] =
    {
        0, 1, 2,    0, 2, 3,
        4, 5, 6,    4, 6, 7,
        8, 9, 10,   8, 10, 11,
        12, 13, 14, 12, 14, 15,
        16, 17, 18, 16, 18, 19,
        20, 21, 22, 20, 22, 23
    };
	Mesh* pMesh = new Mesh();
	
    if (!pMesh->Create(pDevice, positions, normals, colors, uvs,tangents,_countof(positions),indices,_countof(indices)))
    {
        delete pMesh;
        return nullptr;
    }
	return pMesh;
}

Mesh* Mesh::CreateQuad(ID3D11Device* pDevice, float size) {
    // サイズから半分の幅を計算 
    float half = size * 0.5f;

    // XY平面上の四角形

    DirectX::XMFLOAT3 positions[] =
    {
        { -half,  half, 0.0f }, // 左上
        {  half,  half, 0.0f }, // 右上
        {  half, -half, 0.0f }, // 右下
        { -half, -half, 0.0f }  // 左下
    };

    DirectX::XMFLOAT3 normals[] =
    {
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, -1.0f }
    };

    DirectX::XMFLOAT4 colors[] =
    {
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    };

    DirectX::XMFLOAT2 uvs[] =
    {
        { 0.0f, 0.0f }, // 左上
        { 1.0f, 0.0f }, // 右上
        { 1.0f, 1.0f }, // 右下
        { 0.0f, 1.0f }  // 左下
    };

    DirectX::XMFLOAT3 tangents[] =
    {
        { 1.0f,  0.0f,  0.0f }, // 左上頂点に対する接線
        { 1.0f,  0.0f,  0.0f }, // 右上頂点に対する接線
        { 1.0f,  0.0f,  0.0f }, // 右下頂点に対する接線
        { 1.0f,  0.0f,  0.0f }  // 左下頂点に対する接線
    };

    // 時計回りが表面（D3D11のデフォルト）となるようにインデックスを設定
    DWORD indices[] =
    {
        0, 1, 2,  // 1つ目の三角形（左上→右上→右下）
        0, 2, 3   // 2つ目の三角形（左上→右下→左下）
    };

    Mesh* pMesh = new Mesh();
    if (!pMesh->Create(pDevice, positions, normals, colors, uvs,tangents,_countof(positions), indices, _countof(indices)))
    {
        delete pMesh;
        return nullptr;
    }
    return pMesh;
}

void Mesh::RenderInstanced(ID3D11DeviceContext* context, UINT instanceCount, ID3D11Buffer* pInstanceBuffer, UINT instanceStride)
{
    // 1~4個目は既存のメンババッファ、5個目に引数のインスタンスバッファをセット
    ID3D11Buffer* vbs[5] = {
        m_pPosBuffer,
        m_pNrmBuffer,
        m_pColorBuffer,
        m_pUvBuffer,
        pInstanceBuffer
    };

    // それぞれのバッファの1要素のバイトサイズ（ストライド）
    UINT strides[5] = {
        sizeof(DirectX::XMFLOAT3), // Position
        sizeof(DirectX::XMFLOAT3), // Normal
        sizeof(DirectX::XMFLOAT4), // Color
        sizeof(DirectX::XMFLOAT2), // UV
        instanceStride             // InstanceData のサイズ
    };

    UINT offsets[5] = { 0, 0, 0, 0, 0 };

    // 5個のバッファを一括バインド
    context->IASetVertexBuffers(0, 5, vbs, strides, offsets);
    context->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // インスタンシング用のドローコールを実行
    context->DrawIndexedInstanced(m_indexCount, instanceCount, 0, 0, 0);
}