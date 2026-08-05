#include"depthStencilStates.h"

bool DepthStencilStates::Initialize(ID3D11Device* device) {
	HRESULT hr;
	// DepthTest
	D3D11_DEPTH_STENCIL_DESC depthTestDesc = {};
	depthTestDesc.DepthEnable = TRUE;
	depthTestDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthTestDesc.DepthFunc = D3D11_COMPARISON_LESS;
	hr=device->CreateDepthStencilState(&depthTestDesc, m_pDepthTestState.GetAddressOf());
	if (FAILED(hr))return false;
	// DepthReadOnly
	D3D11_DEPTH_STENCIL_DESC depthReadOnlyDesc = {};
	depthReadOnlyDesc.DepthEnable = TRUE;
	depthReadOnlyDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 書き込みなし
	depthReadOnlyDesc.DepthFunc = D3D11_COMPARISON_LESS;
	hr=device->CreateDepthStencilState(&depthReadOnlyDesc, m_pDepthReadOnlyState.GetAddressOf());
	if (FAILED(hr))return false;
	// None
	D3D11_DEPTH_STENCIL_DESC noneDesc = {};
	noneDesc.DepthEnable = FALSE; // 深度テスト無効
	hr=device->CreateDepthStencilState(&noneDesc, m_pNoneState.GetAddressOf());
	if (FAILED(hr))return false;
	
	// DepthLessEqual (スカイボックス用：書き込みなし、判定は以下)
	D3D11_DEPTH_STENCIL_DESC skyboxDesc = {};
	skyboxDesc.DepthEnable = TRUE;
	skyboxDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 空の向こうに物は描画されないので書き込み不要
	skyboxDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;     // 1.0で並んだときにパスさせるために必須

	hr = device->CreateDepthStencilState(&skyboxDesc, m_pDepthLessEqualState.GetAddressOf());
	if (FAILED(hr)) return false;

	return true;
}

void DepthStencilStates::Bind(ID3D11DeviceContext* pContext,Mode mode) {
	// モードに応じて適切なステートをバインド
	switch (mode) {
	case Mode::DepthTest:
		pContext->OMSetDepthStencilState(m_pDepthTestState.Get(), 0);
		break;
	case Mode::DepthReadOnly:
		pContext->OMSetDepthStencilState(m_pDepthReadOnlyState.Get(), 0);
		break;
	case Mode::None:
		pContext->OMSetDepthStencilState(m_pNoneState.Get(), 0);
		break;
	case Mode::DepthLessEqual:
		pContext->OMSetDepthStencilState(m_pDepthLessEqualState.Get(),0);
		break;
	}
	return;
}