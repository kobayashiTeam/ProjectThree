// ScreenBlitMaterial.cpp
#include "screenBlitMaterial.h"
#include"renderTarget.h"

// ==========================================================
// ScreenBlitMaterial の実装
// ==========================================================

// スロット2用の定数バッファを生成する
bool ScreenBlitMaterial::CreateMaterialBuffer(ID3D11Device* pDevice)
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
//void ScreenBlitMaterial::Bind(ID3D11DeviceContext* pContext)
//{
//    if (!pContext) return;
//
//    // 1. まず基底クラス（Material）のBindを明示的に呼び出す
//    //    これで共通のシェーダー切り替え、テクスチャ、サンプラーがバインドされます
//    Material::Bind(pContext);
//
//    // 2. 自分専用（ScreenBlitMaterial）のデータを定数バッファに書き込んで、スロット2にセット
//    if (m_pMaterialBuffer)
//    {
//        // CPU側の変更（SetMaterialColor等）をGPU側バッファに転送
//        pContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &m_cbData, 0, 0);
//
//        // ピクセルシェーダーの「スロット2 (b2)」にこのバッファをバインド
//        pContext->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
//    }
//}

bool ScreenBlitMaterial::initializeScreenBlit(ID3D11Device* pDevice, Shader* pShader) {
    m_pShader = pShader;
    if (!m_pShader) return false;

    // ★テクスチャの新規作成処理はすべてカット！★
    // なぜなら、描画時に外部の RenderTarget から直接 SRV をもらうから。
    m_pTextureRV = nullptr; // 初期化時点では空っぽでOK

    // サンプラー（色の補正ルール）だけは必要なので、ここだけ親のコピペ、
    // あるいはフィルターをPOINTからLINEARに変えるなどして作成する
    HRESULT hr;
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // ポストプロセスはバイリニアが綺麗
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;  // 画面端のバグを防ぐためCLAMPが鉄板
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = pDevice->CreateSamplerState(&sampDesc, &m_pSamplerLinear);
    if (FAILED(hr)) return false;

    return true;
}

void ScreenBlitMaterial::BindScreenBlit(ID3D11DeviceContext* pContext,RenderTarget* sourceRT) {
    if (!pContext || !sourceRT) return;

    // 1. まず基底クラス（Material）のBindを明示的に呼び出す
    //    これで共通のシェーダー切り替え、テクスチャ、サンプラーがバインドされます
    Material::Bind(pContext);

    // 2. 【ここが核心】引数で受け取った「今完成した景色」のSRVをコンテキストに直接セット！
    ID3D11ShaderResourceView* srv = sourceRT->GetSRV();
    pContext->PSSetShaderResources(0, 1, &srv);
}