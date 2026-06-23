#include"instancedModel.h"
#include"material.h"
#include"mesh.h"

void InstancedModel::Render(ID3D11DeviceContext* pContext) {

    if (m_instanceData.empty() || !m_pInstanceBuffer) return;

    // ★GPUへの転送ロジック（Map/Unmap）がここに混ざる
    UINT count = (std::min)(
        static_cast<UINT>(m_instanceData.size()),
        m_maxInstances
        );
    D3D11_MAPPED_SUBRESOURCE mappedResource = {};
    if (SUCCEEDED(pContext->Map(m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        memcpy(mappedResource.pData, m_instanceData.data(), sizeof(InstanceData) * count);
        pContext->Unmap(m_pInstanceBuffer, 0);
    }

    // 2. マテリアルバインド:shaderは内部生成の物を使うことになった
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
    //UINT count = m_instanceData.size();
    m_pMesh->RenderInstanced(pContext, count, m_pInstanceBuffer, sizeof(InstanceData));
}


bool InstancedModel:: Init(ID3D11Device* pDevice, ID3D11DeviceContext* pContext,
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
    hr = pDevice->CreateInputLayout(instancedLayout, layoutCount, pVSBlob->GetBufferPointer(), //第二引数4
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
    //一応マテリアルを初期化する
    SetMaterialColor(1,1,1,1);


    //test:行列リソースをここで作っておく
    std::vector<InstanceData> instances;
    const int N = 3;
    const float spacing = 2.5f;
    for (int z = 0; z < N; ++z)
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
            {
                float px = (x - N / 2) * spacing;
                float py = (y - N / 2) * spacing;
                float pz = (z - N / 2) * spacing;

                // インスタンスごとに位相をずらして回転
                //float phase = (x + y * N + z * N * N) * 0.3f;
                //DirectX::XMMATRIX rot = XMMatrixRotationY(time + phase);
                DirectX::XMMATRIX world = DirectX::XMMatrixTranslation(px, py, pz);

                // ★ XMMatrixMultiply は「左から右に適用」
                //   rot * trans = ローカル回転してからワールド平行移動
                //DirectX::XMMATRIX world = XMMatrixMultiply(rot, trans);

                // ★ シェーダへ送る前に転置が必要か？
                //   HLSLのmul(vec,mat)は「行ベクトル × 行列」なので、
                //   XMMatrixを「そのまま行ごとにfloat4x4に書き出す」だけでOK。
                //   （XMStoreFloat4x4 → row0=_11~_14 の順で格納）
                m_instanceData.push_back(MatrixToInstanceData(world));
            }
    /*DirectX::XMMATRIX world = DirectX::XMMatrixTranslation(5.5f, 0.5f, 10.5f);
    InstanceData changedWorld = MatrixToInstanceData(world);
    AddInstance(MatrixToInstanceData(world));*/
    UINT count = (UINT)instances.size();
        //c++でつくったデータを送る
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(pContext->Map(m_pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, m_instanceData.data(), sizeof(InstanceData)*count);
        pContext->Unmap(m_pInstanceBuffer, 0);
    }


    return true;
}
