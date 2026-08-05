#include"ssaoPass.h"
#include<random>

bool SSAOPass::initNoiseTexture(ID3D11Device* pDevice) {

    ID3D11Texture2D* m_pNoiseTexture = nullptr;
    // 1. もとになるノイズ色（ベクトル）配列をプログラム内で生成
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    // 4x4 = 16画素分のデータ。高精度なFLOATフォーマットを使うため DirectX::XMFLOAT4 を使用
    DirectX::XMFLOAT4 noiseValues[16];
    for (int i = 0; i < 16; ++i)
    {
        noiseValues[i].x = dis(gen); // -1.0 ～ 1.0 のランダム
        noiseValues[i].y = dis(gen); // -1.0 ～ 1.0 のランダム
        noiseValues[i].z = 0.0f;     // 接空間のZ（法線方向）は回転させないので 0
        noiseValues[i].w = 0.0f;     // 未使用
    }

    // 2. テクスチャの設定（ID3D11Texture2D）を定義
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 4;                       // 4マス
    texDesc.Height = 4;                       // 4マス
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // 浮動小数点の高精度フォーマット
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_DEFAULT;     // GPUから読み込むだけなのでDEFAULT
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // シェーダーに渡す用
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    // 作成と同時に、さっき作った配列データを初期データとして流し込む
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = noiseValues;
    initData.SysMemPitch = 4 * sizeof(DirectX::XMFLOAT4); // 横1行分のバイト数
    initData.SysMemSlicePitch = 0;

    HRESULT hr = pDevice->CreateTexture2D(&texDesc, &initData, &m_pNoiseTexture);
    if (FAILED(hr)) return false;

    // 3. テクスチャをもとにSRVを定義・作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = pDevice->CreateShaderResourceView(m_pNoiseTexture, &srvDesc, &m_noiseTextureSRV);
    if (FAILED(hr)) return false;

    return true;
}

bool SSAOPass::initSamplers(ID3D11Device* pDevice) {

    HRESULT hr = S_OK;
    D3D11_SAMPLER_DESC sampDesc = {};

    // ---------------------------------------------------------
    // ① samPointClamp (点サンプリング / 範囲外クランプ)
    // ---------------------------------------------------------
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // Point指定
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;    // Clamp指定
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_samPointClamp);
    if (FAILED(hr)) return false;

    // ---------------------------------------------------------
    // ② samPointWrap (点サンプリング / 範囲外ラップ・タイリング)
    // ---------------------------------------------------------
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // Point指定
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;     // Wrap指定 (タイリング用)
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_samPointWrap);
    if (FAILED(hr)) return false;

    // ---------------------------------------------------------
    // ③ samLinearClamp (線形サンプリング / 範囲外クランプ)
    // ---------------------------------------------------------
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // Linear指定 (ぼかし用)
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;     // Clamp指定
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_samLinearClamp);
    if (FAILED(hr)) return false;

    return true;

}

bool SSAOPass::initConstantBuffer(ID3D11Device* pDevice, float w, float h) {

    //ここで実際の画面解像度（ビューポートの横幅・縦幅）を使って計算する
    float windowWidth = 1280.0f; // 実際のゲーム画面の横幅
    float windowHeight = 720.0f; // 実際のゲーム画面の縦幅
    // --- 64個のサンプルベクトルの生成ロジック ---
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    std::uniform_real_distribution<float> disZ(0.0f, 1.0f); // Zは半球なので0～1

    for (int i = 0; i < 64; ++i)
    {
        DirectX::XMVECTOR sample = DirectX::XMVectorSet(dis(gen), dis(gen), disZ(gen), 0.0f);
        sample = DirectX::XMVector3Normalize(sample); // 正規化

        // 中心に近づくほど密集させるスケール処理
        float scale = (float)i / 64.0f;
        // 線形補間 (0.1f ～ 1.0f の間で二次関数的に配分)
        scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
        sample = DirectX::XMVectorScale(sample, scale);

        DirectX::XMStoreFloat4(&m_paramData.samples[i], sample);
    }

    // --- 固定パラメータの初期値設定 ---
    m_paramData.noiseScale = DirectX::XMFLOAT2(windowWidth / 4.0f, windowHeight / 4.0f); // 画面解像度に合わせて後で更新も可
    m_paramData.radius = 0.40f;   // 遮蔽を調べる半径（ゲームのスケールに合わせて要調整）
    m_paramData.bias = 0.03f; // アクネ（自己遮蔽ノイズ）を防ぐためのバイアス値

    // --- Dynamic定数バッファの作成 ---
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(SSAOParam);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;         // CPUから頻繁に書き換える
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;  // 定数バッファとして使用
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;       // CPUからの書き込み許可
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    HRESULT hr = pDevice->CreateBuffer(&cbDesc, nullptr, &m_ssaoCB);
    if (FAILED(hr)) return false;

    return true;

}

void SSAOPass::updateConstantBuffer(ID3D11DeviceContext* ctx) {

    // TODO: 将来的にImGui等でradius/biasを実行時調整する場合は、ここでm_paramDataを更新してからMapする

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    // GPUの書き込みが終わるのを待たずに新しいバッファを割り当てる DISCARD を指定
    HRESULT hr = ctx->Map(m_ssaoCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        // CPU側のデータをバッファへコピー
        memcpy(mappedResource.pData, &m_paramData, sizeof(SSAOParam));
        ctx->Unmap(m_ssaoCB.Get(), 0);
    }

    // ピクセルシェーダーのスロット 6 に定数バッファをセット
    ctx->PSSetConstantBuffers(6, 1, m_ssaoCB.GetAddressOf());

}
