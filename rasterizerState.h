// Graphics/RasterizerState.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h> // ComPtr ‚Ì‚½‚ß‚É•K—v
using Microsoft::WRL::ComPtr;

class RasterizerState {
public:
    enum class CullMode {
        Back,
        Front,
        None
    };
	RasterizerState() = default;
	~RasterizerState() = default;
    bool Initialize(ID3D11Device* device,CullMode mode);
    void Bind(ID3D11DeviceContext* ctx);

private:
    ComPtr<ID3D11RasterizerState> m_state;
};