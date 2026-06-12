#include"rasterizerState.h"

bool RasterizerState::Initialize(ID3D11Device* device,CullMode mode) {
	D3D11_RASTERIZER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	// 共通の基本設定
	desc.FillMode = D3D11_FILL_SOLID;
	desc.CullMode = D3D11_CULL_BACK;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthClipEnable = TRUE;

	if (mode == CullMode::Back) {
		desc.CullMode = D3D11_CULL_BACK;
	}
	else if (mode == CullMode::Front) {
		desc.CullMode = D3D11_CULL_FRONT;
	}
	else if (mode == CullMode::None) {
		desc.CullMode = D3D11_CULL_NONE;
	}
	else {
		return false; // 不正なモード
	}

	// デバイスに説明書を渡してVRAM上に生成
	HRESULT hr = device->CreateRasterizerState(&desc, m_state.GetAddressOf());
	if (FAILED(hr)) return false;

	return true;
}

void RasterizerState::Bind(ID3D11DeviceContext* ctx) {
	ctx->RSSetState(m_state.Get());
}