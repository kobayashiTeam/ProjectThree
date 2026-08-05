#pragma once
#include <d3d11.h>
#include <wrl/client.h>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class RasterizerStates {
public:
    enum class CullMode { Back, Front, None };

    bool Initialize(ID3D11Device* device);

    void Bind(ID3D11DeviceContext* ctx, CullMode mode);

    ID3D11RasterizerState* Get(CullMode mode);

private:
    ComPtr<ID3D11RasterizerState> m_backSolid;
    ComPtr<ID3D11RasterizerState> m_frontSolid;
    ComPtr<ID3D11RasterizerState> m_noneSolid;
};