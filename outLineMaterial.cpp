// OutLineMaterial.cpp
#include "outLineMaterial.h"

// ==========================================================
// OutLineMaterial の実装
// ==========================================================

// スロット2用の定数バッファを生成する
bool OutLineMaterial::CreateMaterialBuffer(ID3D11Device* pDevice)
{
    if (!pDevice) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerMaterialCB); // 16バイト (XMFLOAT4 1個分で16の倍数)
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, &m_pMaterialBuffer);
    return SUCCEEDED(hr);
}

// 描画時に呼ばれるバインド処理
void OutLineMaterial::Bind(ID3D11DeviceContext* pContext)
{
    if (!pContext) return;

    // 1. まず基底クラス（Material）のBindを明示的に呼び出す
    //    これで共通のシェーダー切り替え、テクスチャ、サンプラーがバインドされます
    Material::Bind(pContext);

    // 2. 自分専用（OutLineMaterial）のデータを定数バッファに書き込んで、スロット2にセット
    if (m_pMaterialBuffer)
    {
        // CPU側の変更（SetMaterialColor等）をGPU側バッファに転送
        pContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &m_cbData, 0, 0);

        // ピクセルシェーダーの「スロット2 (b2)」にこのバッファをバインド
        pContext->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
    }
}