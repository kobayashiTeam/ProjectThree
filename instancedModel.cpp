#include"instancedModel.h"
#include"material.h"
#include"mesh.h"
#include<cmath>

void InstancedModel::Render(ID3D11DeviceContext* pContext) {

    if (m_instanceData.empty() || !m_pInstanceBuffer) return;

    // インスタンスデータをGPU用バッファへ転送（Map/Unmap）
    UINT count = (std::min)(
        static_cast<UINT>(m_instanceData.size()),
        m_maxInstances
        );
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    if (SUCCEEDED(pContext->Map(m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        memcpy(mappedResource.pData, m_instanceData.data(), sizeof(InstanceData) * count);
        pContext->Unmap(m_pInstanceBuffer, 0);
    }

    // 2. マテリアルバインド（シェーダーはこのクラス内で生成したものを使用）
    pContext->IASetInputLayout(m_pVertexLayout);
    pContext->VSSetShader(m_pVertexShader, nullptr, 0);
    pContext->PSSetShader(m_pPixelShader, nullptr, 0);
    //自前用意したsrv,sampler,materialCBもバインド
    if (m_pMaterialBuffer)
    {
        // CPU側の変更（SetMaterialColor等）をGPU側バッファに転送
        pContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &m_cbData, 0, 0);
        // ピクセルシェーダーの「スロット2 (b2)」にこのバッファをバインド
        pContext->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
    }
    pContext->PSSetShaderResources(0, 1, &m_pTextureRV);
    pContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

    // 3. 描画（直接バッファのポインタとストライドのサイズを渡す）
    m_pMesh->RenderInstanced(pContext, count, m_pInstanceBuffer, sizeof(InstanceData));
}


bool InstancedModel::Init(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
    Mesh* mesh,UINT maxInstances) {
    m_pMesh = mesh;
    m_maxInstances = maxInstances;

    //vbのスロット4に送るデータ、そのバッファを作成
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(InstanceData) * m_maxInstances;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(&desc, nullptr, &m_pInstanceBuffer);

    // あらかじめ最大数分のメモリ領域を確保して、毎フレームの push_back による再確保のオーバーヘッドを減らす
    m_instanceData.reserve(m_maxInstances);

    //シェーダ作成
    D3D11_INPUT_ELEMENT_DESC instancedLayout[] = {
        // スロット0 ~ 3：既存のMeshに合わせた通常の頂点データ（PER_VERTEX）
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        // スロット4：インスタンスバッファから1行ずつ（16バイトずつ）パース（PER_INSTANCE）
        { "INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };
    UINT layoutCount = sizeof(instancedLayout) / sizeof(D3D11_INPUT_ELEMENT_DESC);

    HRESULT hr;
    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;
    const wchar_t* fileName = L"Shaders/Lit_InstancingShader.hlsl";

        // 1. 頂点シェーダーのコンパイルと生成
    hr = D3DCompileFromFile(fileName, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    hr = pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }

         // 2. 頂点レイアウトの作成（現状のレイアウトをそのまま移植）
    hr = pDevice->CreateInputLayout(instancedLayout, layoutCount, pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(), &m_pVertexLayout);
    pVSBlob->Release();
    if (FAILED(hr)) return false;

        // 3. ピクセルシェーダーのコンパイルと生成
    ID3DBlob* pPSBlob = nullptr;
    hr = D3DCompileFromFile(fileName, nullptr, nullptr, "PS", "ps_5_0", 0, 0,
        &pPSBlob, &pErrorBlob);
    if (FAILED(hr)) { if (pErrorBlob) pErrorBlob->Release(); return false; }

    hr = pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
        pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    pPSBlob->Release();
    if (FAILED(hr)) return false;

    //テクスチャの作成
    UINT32 white = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA tInitData = {};
    tInitData.pSysMem = &white;
    tInitData.SysMemPitch =  sizeof(UINT32);

    ID3D11Texture2D* pTexture2D = nullptr;
    hr = pDevice->CreateTexture2D(&td, &tInitData, &pTexture2D);
    if (FAILED(hr)) return false;

    hr = pDevice->CreateShaderResourceView(pTexture2D, nullptr, &m_pTextureRV);
    pTexture2D->Release(); // SRVを作ったら本体はリリースしてOK
    if (FAILED(hr)) return false;


    // サンプラーの作成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
    if (FAILED(hr)) return false;

    //マテリアルバッファ
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerMaterialCB); // 16バイト (XMFLOAT4 1個分で16の倍数)
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    hr = pDevice->CreateBuffer(&cbd, nullptr, &m_pMaterialBuffer);
    if (FAILED(hr))return false;
    // マテリアルの初期化（デフォルトは白色）
    SetMaterialColor(1,1,1,1);


    // Scene7用：maxInstances分の座標を最初に1回だけ計算してテーブル化しておく
    // （オブジェクトプールと同じ発想：資源は先に確保、使う数だけ後から選ぶ）
    // 一辺の長さは maxInstances に収まる最小の立方体サイズ（512なら8×8×8）
    const int side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(m_maxInstances))));
    const float spacing = 2.5f;

    m_positionTable.reserve(m_maxInstances);
    for (int z = 0; z < side && m_positionTable.size() < m_maxInstances; ++z)
        for (int y = 0; y < side && m_positionTable.size() < m_maxInstances; ++y)
            for (int x = 0; x < side && m_positionTable.size() < m_maxInstances; ++x)
            {
                float px = (x - side / 2) * spacing;
                float py = (y - side / 2) * spacing;
                float pz = (z - side / 2) * spacing;

                DirectX::XMMATRIX world = DirectX::XMMatrixTranslation(px, py, pz);
                m_positionTable.push_back(MatrixToInstanceData(world));
            }

    // 原点に近い順に並べ替え（個数が少ないうちはカメラの近くに出るように）
    std::sort(m_positionTable.begin(), m_positionTable.end(),
        [](const InstanceData& a, const InstanceData& b) {
            auto distSq = [](const InstanceData& d) {
                float x = d.row3.x, y = d.row3.y, z = d.row3.z;
                return x * x + y * y + z * z;
                };
            return distSq(a) < distSq(b);
        });

    // m_instanceDataは空のまま開始（他のシーンで無関係なcubeが映り込まないように）
    // 実際に何個表示するかはScene7側からSetActiveCount()で指定する
    return true;
}

void InstancedModel::SetActiveCount(UINT count) {
    if (count > m_maxInstances) count = m_maxInstances;

    m_instanceData.clear();
    m_instanceData.insert(m_instanceData.end(),
        m_positionTable.begin(), m_positionTable.begin() + count);
}

void InstancedModel::SetActiveCount(UINT count, const DirectX::XMFLOAT3& offset) {
    if (count > m_maxInstances) count = m_maxInstances;

    m_instanceData.clear();
    m_instanceData.insert(m_instanceData.end(),
        m_positionTable.begin(), m_positionTable.begin() + count);

    // 回転を含まない平行移動行列のため、row3への加算だけで群全体を移動できる
    for (auto& inst : m_instanceData) {
        inst.row3.x += offset.x;
        inst.row3.y += offset.y;
        inst.row3.z += offset.z;
    }
}