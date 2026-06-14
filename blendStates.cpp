#include"blendStates.h"

bool BlendStates::Initialize(ID3D11Device* device)
{
    HRESULT hr;

    // ====================== None (ブレンド無効) ======================
    D3D11_BLEND_DESC noneDesc = {};  // 重要：{} でゼロクリア
    noneDesc.RenderTarget[0].BlendEnable = FALSE;
    noneDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&noneDesc, m_noneState.GetAddressOf());
    if (FAILED(hr)) return false;

    // ====================== Alpha (通常の半透明) ======================
    D3D11_BLEND_DESC alphaDesc = {};
    alphaDesc.RenderTarget[0].BlendEnable = TRUE;
    alphaDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    alphaDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    alphaDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    alphaDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    alphaDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    alphaDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    alphaDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&alphaDesc, m_alphaState.GetAddressOf());
    if (FAILED(hr)) return false;

    // ====================== Additive (光の加算など) ======================
    D3D11_BLEND_DESC additiveDesc = {};
    additiveDesc.RenderTarget[0].BlendEnable = TRUE;
    additiveDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;        // ← ここをONEに変更
    additiveDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;       // 背景にそのまま足す
    additiveDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    additiveDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;  // または ZERO
    additiveDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    additiveDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&additiveDesc, m_additiveState.GetAddressOf());
    if (FAILED(hr))
    {
        // デバッグ用にhrの値を確認したい場合はここにOutputDebugStringなど
        return false;
    }

    return true;
}

void BlendStates::Bind(ID3D11DeviceContext* context,BlendMode mode) {
	//代入されたモードに応じて適切なステートをバインド
	switch (mode) {
	case BlendMode::Opaque:
		context->OMSetBlendState(m_noneState.Get(), nullptr, 0xffffffff);
		break;
	case BlendMode::AlphaBlend:
		context->OMSetBlendState(m_alphaState.Get(), nullptr, 0xffffffff);
		break;
	case BlendMode::Additive:
		context->OMSetBlendState(m_additiveState.Get(), nullptr, 0xffffffff);
		break;
	}

	return;
}