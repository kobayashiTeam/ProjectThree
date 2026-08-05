#pragma once
#include <d3d11.h>
#include <wrl/client.h>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class DepthStencilStates {
public:
	enum class Mode {
		DepthTest,      // 深度テスト有効
		DepthReadOnly,  // 深度テストはするが、書き込みはしない（例：半透明オブジェクト用）
		None,            // 深度テスト無効
		DepthLessEqual  // 深度比較を LessEqual に設定（Skybox 用など）
	};
	bool Initialize(ID3D11Device* device);
	void Bind(ID3D11DeviceContext* pContext,Mode mode);
private:
	// 各モードに対応する深度ステート
	ComPtr<ID3D11DepthStencilState> m_pDepthTestState;
	ComPtr<ID3D11DepthStencilState> m_pDepthReadOnlyState;
	ComPtr<ID3D11DepthStencilState> m_pNoneState;
	ComPtr<ID3D11DepthStencilState> m_pDepthLessEqualState;

};