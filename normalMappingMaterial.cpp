// NormalMappingMaterial.cpp
#include "normalMappingMaterial.h"
#include <directxtk/WICTextureLoader.h>

// ==========================================================
// NormalMappingMaterial の実装
// ==========================================================

// スロット2用の定数バッファを生成する
bool NormalMappingMaterial::CreateMaterialBuffer(ID3D11Device* pDevice)
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
void NormalMappingMaterial::Bind(ID3D11DeviceContext* pContext)
{
    if (!pContext) return;

    // 1. まず基底クラス（Material）のBindを明示的に呼び出す
    //    これで共通のシェーダー切り替え、テクスチャ、サンプラーがバインドされます
    Material::Bind(pContext);

    // 2. materialのデータを定数バッファに書き込んで、スロット2にセット
    if (m_pMaterialBuffer)
    {
        // CPU側の変更（SetMaterialColor等）をGPU側バッファに転送
        pContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &m_cbData, 0, 0);

        // ピクセルシェーダーの「スロット2 (b2)」にこのバッファをバインド
        pContext->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
    }

	//3. NormalMapのSRVとSamplerをピクセルシェーダーにセット
    if (m_pNormalMapTextureRV) {
        pContext->PSSetShaderResources(1, 1, &m_pNormalMapTextureRV);
    }
}


bool NormalMappingMaterial:: 
InitializeNormalMapFromFile(ID3D11Device* pDevice, const wchar_t* pFileName) {

    HRESULT // 派生クラスの 法線マップ 読み込み（通常データとして強制指定）
        hr = DirectX::CreateWICTextureFromFileEx(
            pDevice, pFileName, 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
            DirectX::WIC_LOADER_IGNORE_SRGB, // ★法線マップは絶対にsRGBにしない！
            nullptr, &m_pNormalMapTextureRV
        );
    if (FAILED(hr)) {
        OutputDebugString(L"Failed to load normal map file: ");
        OutputDebugString(pFileName);
        OutputDebugString(L"\n");
        return false;
    }
    return true;
}