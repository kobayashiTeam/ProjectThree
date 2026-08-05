#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "graphicsCommon.h"

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class BlendStates {
public:
	
	bool Initialize(ID3D11Device* device);
	void Bind(ID3D11DeviceContext* context,BlendMode mode);

private:
	// 各ブレンドモードに対応するブレンドステート
	ComPtr<ID3D11BlendState> m_noneState;
	ComPtr<ID3D11BlendState> m_alphaState;
	ComPtr<ID3D11BlendState> m_additiveState;

};