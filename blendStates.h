#pragma once
// BlendStates.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class BlendStates {
public:
	enum class Mode {
		None,       // ブレンド無効（不透明）
		Alpha,      // 半透明
		Additive    // 加算合成（エフェクト用）
	};

	bool Initialize(ID3D11Device* device);
	void Bind(ID3D11DeviceContext* context,Mode mode);

private:
	//モード３つ分ステートを生成したもの別々に保管
	ComPtr<ID3D11BlendState> m_noneState;
	ComPtr<ID3D11BlendState> m_alphaState;
	ComPtr<ID3D11BlendState> m_additiveState;

};