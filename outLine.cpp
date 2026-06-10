#include"outLine.h"
#include"model.h"

OutLine::OutLine() {
}

OutLine::~OutLine() {
	if (m_pNormalStencilState) {
		m_pNormalStencilState->Release();
		m_pNormalStencilState = nullptr;
	}
	if (m_pOutlineStencilState) {
		m_pOutlineStencilState->Release();
		m_pOutlineStencilState = nullptr;
	}
}

bool OutLine::createStencilState(ID3D11Device* pDevice, ID3D11DeviceContext* context) {

    HRESULT hr;
    //書き込み用ステート
    D3D11_DEPTH_STENCIL_DESC normalStencilDesc = {};
    normalStencilDesc.DepthEnable = TRUE;
    normalStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    normalStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

    normalStencilDesc.StencilEnable = TRUE;
    normalStencilDesc.StencilReadMask = 0xFF;
    normalStencilDesc.StencilWriteMask = 0xFF;

    // 前面・背面とも同じ設定でOK（シンプルに）
    normalStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    normalStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    normalStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;  // 重要
    normalStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    normalStencilDesc.BackFace = normalStencilDesc.FrontFace;  // 同じ設定

    hr = pDevice->CreateDepthStencilState(&normalStencilDesc, &m_pNormalStencilState);
    if (FAILED(hr)) return false;

    //読み込み用ステート
    D3D11_DEPTH_STENCIL_DESC outlineStencilDesc = {};
    outlineStencilDesc.DepthEnable = FALSE;           // 重要：深度テスト無効
    outlineStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    outlineStencilDesc.StencilEnable = TRUE;
    outlineStencilDesc.StencilReadMask = 0xFF;
    outlineStencilDesc.StencilWriteMask = 0x00;       // 書き込みはしない

    outlineStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    outlineStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    outlineStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    outlineStencilDesc.FrontFace.StencilFunc =
        static_cast<D3D11_COMPARISON_FUNC>(D3D11_COMPARISON_NOT_EQUAL);;  // 1以外なら描画

    outlineStencilDesc.BackFace = outlineStencilDesc.FrontFace;

    pDevice->CreateDepthStencilState(&outlineStencilDesc, &m_pOutlineStencilState);

    return true;
}

void OutLine::DrawOutline(ID3D11DeviceContext* pContext, Model* pModel, ID3D11Buffer* pCB) {

    // === Pass 1 ===
    pContext->OMSetDepthStencilState(m_pNormalStencilState, 1);
    pModel->SetScale(1.0f, 1.0f, 1.0f);
    pModel->ResetMaterialOverride();
    pModel->Draw(pContext, pCB);

    // === Pass 2 ===
    pContext->OMSetDepthStencilState(m_pOutlineStencilState, 1);
    pModel->SetScale(1.2f, 1.2f, 1.2f);
    pModel->SetMaterialOverride(m_pOutlineMaterial);  
    pModel->Draw(pContext, pCB);

    // === 後片付け ===
    pModel->SetScale(1.0f, 1.0f, 1.0f);
    pModel->ResetMaterialOverride();
}