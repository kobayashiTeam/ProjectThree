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

Mesh* Mesh::CreateCube(ID3D11Device* pDevice, float size) {
	// 8頂点の立方体(24頂点、4*6)
    SimpleVertex vertices[] =
    {
        { -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 1.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 0.0f },
        {  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f },
        { -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 0.0f },
        {  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 1.0f },
        { -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 0.0f },
        { -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f },
        { -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 1.0f },
        {  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 0.0f },
        {  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 0.0f },
        {  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  1.0f, 1.0f },
        {  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,1.0f,  0.0f, 1.0f },
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
	if (!pMesh->Create(pDevice, vertices, _countof(vertices), indices, _countof(indices)))
	{
		delete pMesh;
		return nullptr;
	}
	return pMesh;
}

Mesh* Mesh::CreateQuad(ID3D11Device* pDevice, float size) {
    // サイズから半分の幅を計算 (size=2.0f のとき、half=1.0f になり -1.0 〜 1.0 を覆う)
    float half = size * 0.5f;

    // 4頂点で構成される1枚の四角形（XY平面）
    // 構造体の並び：位置(x,y,z), 法線(x,y,z), カラー(r,g,b,a), UV(u,v) と仮定しています
    SimpleVertex vertices[] =
    {
        //    位置 (X, Y, Z)        |    法線 (X, Y, Z)     |        カラー (R, G, B, A)      |   UV (U, V)
        { -half,  half, 0.0f,         0.0f, 0.0f, -1.0f,        1.0f, 1.0f, 1.0f, 1.0f,         0.0f, 0.0f }, // 0: 左上
        {  half,  half, 0.0f,         0.0f, 0.0f, -1.0f,        1.0f, 1.0f, 1.0f, 1.0f,         1.0f, 0.0f }, // 1: 右上
        {  half, -half, 0.0f,         0.0f, 0.0f, -1.0f,        1.0f, 1.0f, 1.0f, 1.0f,         1.0f, 1.0f }, // 2: 右下
        { -half, -half, 0.0f,         0.0f, 0.0f, -1.0f,        1.0f, 1.0f, 1.0f, 1.0f,         0.0f, 1.0f }, // 3: 左下
    };

    // 時計回りが表面（D3D11のデフォルト）となるようにインデックスを設定
    DWORD indices[] =
    {
        0, 1, 2,  // 1つ目の三角形（左上→右上→右下）
        0, 2, 3   // 2つ目の三角形（左上→右下→左下）
    };

    Mesh* pMesh = new Mesh();
    if (!pMesh->Create(pDevice, vertices, _countof(vertices), indices, _countof(indices)))
    {
        delete pMesh;
        return nullptr;
    }
    return pMesh;
}