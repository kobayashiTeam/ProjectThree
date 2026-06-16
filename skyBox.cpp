#include "SkyBox.h"
// DirectXTKのDDSローダーをインクルード（導入方法は後述）
#include <directxtk/DDSTextureLoader.h> 
#include"shaderManager.h"

bool SkyBox::Initialize(ID3D11Device* device, const std::wstring& texturePath) {
    HRESULT hr = S_OK;

    // ==========================================
    // 1. DDSキューブマップテクスチャの読み込み
    // ==========================================
    // DirectXTKの関数を使えば、これだけで6面分のデータを
    // 自動的にキューブマップ構造（SRV）として生成してくれます。
    hr = DirectX::CreateDDSTextureFromFile(
        device,
        texturePath.c_str(),
        nullptr,               // ID3D11Resource** (今回は不要なのでnullptr)
        m_cubeMapSRV.GetAddressOf() // ここに生成されたSRVが格納される
    );

    if (FAILED(hr)) {
        // 読み込み失敗（パスが間違っている、ファイルが壊れているなど）
        OutputDebugStringA("Failed to load Skybox DDS texture.\n");
        return false;
    }


    // ==========================================
    // 2. サンプラーステートの生成
    // ==========================================
    // キューブマップをサンプリング（補間）するための設定です。
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // 線形補間（綺麗に見せる）
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;    // 範囲外はループ（基本はみ出さないが安全のため）
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;    // キューブマップはW軸（3次元）も必要
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = device->CreateSamplerState(&samplerDesc, m_samplerState.GetAddressOf());
    if (FAILED(hr)) return false;

    // (次回以降のステップ：ここに頂点バッファ、インデックスバッファ、シェーダー、ステートの生成が続きます)
    // ==========================================
    // 3. 立方体の頂点データ (位置情報のみ)
    // ==========================================
    // 立方体の8つの頂点座標を定義します。
    // SkyBox::Initialize 内の頂点定義部分
    float size = 500.0f;
    //SkyboxVertex vertices[] = {
    //    // 前面 (Z = 0.5)
    //    { { -0.5f,  0.5f,  0.5f }, {}, {}, {} }, // 左上
    //    { {  0.5f,  0.5f,  0.5f }, {}, {}, {} }, // 右上
    //    { {  0.5f, -0.5f,  0.5f }, {}, {}, {} }, // 右下
    //    { { -0.5f, -0.5f,  0.5f }, {}, {}, {} }, // 左下
    //    // 背面 (Z = -0.5)
    //    { { -0.5f,  0.5f, -0.5f }, {}, {}, {} }, // 左上
    //    { {  0.5f,  0.5f, -0.5f }, {}, {}, {} }, // 右上
    //    { {  0.5f, -0.5f, -0.5f }, {}, {}, {} }, // 右下
    //    { { -0.5f, -0.5f, -0.5f }, {}, {}, {} }  // 左下
    //};
    SkyboxVertex vertices[] = {
        // 前面 (Z = 0.5)
        { { -size,  size,  size }, {}, {}, {} }, // 左上
        { {  size,  size,  size }, {}, {}, {} }, // 右上
        { {  size, -size,  size }, {}, {}, {} }, // 右下
        { { -size, -size,  size }, {}, {}, {} }, // 左下
        // 背面 (Z = -0.5)
        { { -size,  size, -size }, {}, {}, {} }, // 左上
        { {  size,  size, -size }, {}, {}, {} }, // 右上
        { {  size, -size, -size }, {}, {}, {} }, // 右下
        { { -size, -size, -size }, {}, {}, {} }  // 左下
    };

    // ==========================================
    // 4. インデックスデータ (内側から見た三角形の定義)
    // ==========================================
    // ★ここがポイントです：内側から見たときに「時計回り」になるように、
    // インデックスの並び（面の向き）を定義しています。
    uint16_t indices[] = {
        // 前面
        0, 1, 2,  0, 2, 3,
        // 背面 (通常とは逆順にして内側を向ける)
        5, 4, 7,  5, 7, 6,
        // 左面
        4, 0, 3,  4, 3, 7,
        // 右面
        1, 5, 6,  1, 6, 2,
        // 上面
        4, 5, 1,  4, 1, 0,
        // 下面
        3, 2, 6,  3, 6, 7
    };

    // 頂点バッファの生成
    D3D11_BUFFER_DESC vbd{};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vinitData{};
    vinitData.pSysMem = vertices;
    hr = device->CreateBuffer(&vbd, &vinitData, &m_pVertexBuffer);
    if (FAILED(hr)) return false;

    // インデックスバッファの生成
    D3D11_BUFFER_DESC ibd{};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA iinitData{};
    iinitData.pSysMem = indices;
    hr = device->CreateBuffer(&ibd, &iinitData, &m_indexBuffer);
    if (FAILED(hr)) return false;


    // ==========================================
    // 5. 行列転送用定数バッファ (Constant Buffer) の生成
    // ==========================================
    // シェーダーに毎フレーム行列を送り直すため、
    // 頻繁な書き換えに適した「DYNAMIC / WRITE_DISCARD」のバッファを作ります。
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DYNAMIC;              // CPUから毎フレーム書き換える
    cbd.ByteWidth = sizeof(DirectX::XMMATRIX);    // 行列1枚分のサイズ (64バイト)
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;   // 定数バッファとして割り当て
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;   // CPUからの書き込みアクセスを許可

    hr = device->CreateBuffer(&cbd, nullptr, m_constantBuffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // ShaderManagerからスカイボックス用シェーダーの参照を貰う
    // (ShaderManagerがstaticシングルトン等の場合の一例です)
    m_shaderProgram = ShaderManager::GetInstance().GetShader(ShaderID::SkyBox);
    if (!m_shaderProgram) return false;

    return true;
}


void SkyBox::Draw(ID3D11DeviceContext* context,
    const DirectX::XMMATRIX& viewMatrix,
    const DirectX::XMMATRIX& projectionMatrix)
{
    // 完全に安全な描画を行うため、リソースの存在チェック
    if (!m_pVertexBuffer || !m_indexBuffer || !m_shaderProgram || !m_cubeMapSRV) return;

    // =========================================================================
    // 1. 【核心】ビュー行列から平行移動成分（位置情報）を消し去る
    // =========================================================================
    // 4x4行列の右側3マス（_41, _42, _43）がカメラの座標を表しています。
    // ここを 0.0f に書き換えることで、カメラがどれだけ移動しても空が追従します。
    DirectX::XMMATRIX skyboxView = viewMatrix;
    skyboxView.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    // 行列を合成 (View * Projection)
    DirectX::XMMATRIX viewProj = skyboxView * projectionMatrix;
    // D3D11は行列を転置（Transpose）してシェーダーに送る必要があります
    viewProj = DirectX::XMMatrixTranspose(viewProj);

    // 定数バッファを更新して書き込み
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
        memcpy(mappedResource.pData, &viewProj, sizeof(DirectX::XMMATRIX));
        context->Unmap(m_constantBuffer.Get(), 0);
    }

    // =========================================================================
    // 2. ステートのバインド（上書き）
    // =========================================================================
    // レンダラーが保持しているマネジメントクラス、あるいはRenderer自身が用意した
    // 特殊ステート（Cull_Front、Less_Equal）をここでコンテキストにセットします。
    // ※Renderer側で直前にBindしている場合は、ここでの重複Bindは省略可能です。

    // 例：Renderer側でセットされたステートを使う、
    // もしくはSkybox側で直接設定を流し込む場合は以下のように行います。
    // context->RSSetState(m_rasterizerState.Get());       // Cull_Front
    // context->OMSetDepthStencilState(m_depthStencilState.Get(), 0); // Less_Equal

    // =========================================================================
    // 3. パイプラインへのリソース・シェーダーのバインド
    // =========================================================================
    // 頂点バッファとインデックスバッファのセット
    UINT stride = sizeof(SkyboxVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // シェーダーの適用 (自作のShaderクラスのBind処理などを呼ぶ)
    // 内部で IASetInputLayout, VSSetShader, PSSetShader が走る想定です
    m_shaderProgram->Bind(context);

    // 頂点シェーダーに定数バッファをセット（スロット1）//一応１にしてみる
    ID3D11Buffer* cbPtr = m_constantBuffer.Get();
    context->VSSetConstantBuffers(1, 1, &cbPtr);

    // ピクセルシェーダーにキューブマップテクスチャ（SRV）とサンプラーをセット（スロット0）
    ID3D11ShaderResourceView* srvPtr = m_cubeMapSRV.Get();
    ID3D11SamplerState* samplerPtr = m_samplerState.Get();
    context->PSSetShaderResources(0, 1, &srvPtr);
    context->PSSetSamplers(0, 1, &samplerPtr);

    // =========================================================================
    // 4. 描画実行（インデックス数は立方体の 36）
    // =========================================================================
    context->DrawIndexed(36, 0, 0);
}