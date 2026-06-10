#include "mesh.h"

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

bool Mesh::Create(ID3D11Device* pDevice, const SimpleVertex* vertices, 
    UINT vertexCount, const DWORD* indices, UINT indexCount)
{
    //メッシュは最低限、頂点とインデックスのバッファがあればできるのだ
    // 既存のバッファがあれば一度解放
    Cleanup();

    m_indexCount = indexCount;

    // 1. 頂点バッファ（VBO）の作成
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * vertexCount;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;
    HRESULT hr = pDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer);
    if (FAILED(hr)) return false;

    // 2. インデックスバッファ（IBO）の作成
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(DWORD) * indexCount;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initDataIndex = {};
    initDataIndex.pSysMem = indices;
    hr = pDevice->CreateBuffer(&ibd, &initDataIndex, &m_pIndexBuffer);
    if (FAILED(hr)) return false;

    return true;
}

void Mesh::Render(ID3D11DeviceContext* pImmediateContext)
{
    if (!m_pVertexBuffer || !m_pIndexBuffer) return;

    // パイプラインに頂点バッファをセット
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    pImmediateContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

    // インデックスバッファをセット
    pImmediateContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // 描画（トポロジーはメイン側で共通設定にしても良いですが、メッシュが自ら描画します）
    pImmediateContext->DrawIndexed(m_indexCount, 0, 0);
}

void Mesh::Cleanup()
{
    if (m_pIndexBuffer) { m_pIndexBuffer->Release();  m_pIndexBuffer = nullptr; }
    if (m_pVertexBuffer) { m_pVertexBuffer->Release(); m_pVertexBuffer = nullptr; }
    m_indexCount = 0;
}