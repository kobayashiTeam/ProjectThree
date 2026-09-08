// DeferredCBMaterial.cpp
#include "deferredCBMaterial.h"

// ==========================================================
// DeferredCBMaterial の実装
// ==========================================================

// スロット2用の定数バッファを生成する
bool DeferredCBMaterial::CreateMaterialBuffer(ID3D11Device* pDevice)
{
    if (!pDevice) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PerMaterialCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    HRESULT hr = pDevice->CreateBuffer(&cbd, nullptr, &m_pMaterialBuffer);
    return SUCCEEDED(hr);
}

// 描画時に呼ばれるバインド処理
void DeferredCBMaterial::Bind(ID3D11DeviceContext* pContext)
{
    if (!pContext) return;

    // 1. まず基底クラス（Material）のBindを明示的に呼び出す
    //    これで共通のシェーダー切り替え、テクスチャ、サンプラーがバインドされます
    Material::Bind(pContext);

    // 2. 自分専用（DeferredCBMaterial）のデータを定数バッファに書き込んで、スロット2にセット
    if (m_pMaterialBuffer)
    {
        // CPU側の変更（SetMaterialColor等）をGPU側バッファに転送
        pContext->UpdateSubresource(m_pMaterialBuffer, 0, nullptr, &m_cbData, 0, 0);

        // ピクセルシェーダーの「スロット2 (b2)」にこのバッファをバインド
        pContext->PSSetConstantBuffers(2, 1, &m_pMaterialBuffer);
    }
}

// ★追加：複製処理
// 「重い資源（シェーダー・テクスチャ・サンプラー）は共有」「軽いパラメータ（CB）だけ複製」
// という考え方をそのままコードにしたもの
Material* DeferredCBMaterial::Clone(ID3D11Device* pDevice) const
{
    DeferredCBMaterial* clone = new DeferredCBMaterial();

    // --- 重い資源は共有する（複製しない） ---
    clone->m_pShader = m_pShader;       // シェーダーはShaderManagerが管理しているので共有でOK
    clone->m_pGSEffect = m_pGSEffect;   // 任意のジオメトリシェーダーエフェクトも共有

    clone->m_pTextureRV = m_pTextureRV;
    if (clone->m_pTextureRV) clone->m_pTextureRV->AddRef(); // 参照カウントを増やしてから共有

    clone->m_pSamplerLinear = m_pSamplerLinear;
    if (clone->m_pSamplerLinear) clone->m_pSamplerLinear->AddRef();

    // --- 軽いパラメータ（定数バッファ）だけは個別に持つ ---
    // ここを共有してしまうと、また「1つ変えると全部変わる」問題に逆戻りするので
    // 必ず新しいID3D11Bufferを作る
    clone->CreateMaterialBuffer(pDevice);
    clone->m_cbData = m_cbData; // 現時点の色・metallic・roughnessの値を初期値として引き継ぐ

    return clone;
}
