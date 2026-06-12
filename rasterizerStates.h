// Graphics/RasterizerStates.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class RasterizerStates {
public:
    enum class CullMode { Back, Front, None };

    bool Initialize(ID3D11Device* device);

    void Bind(ID3D11DeviceContext* ctx, CullMode mode);

    // ŒÂ•Ê‚Éæ“¾‚µ‚½‚¢ê‡‚à‘Î‰‰Â”\
    ID3D11RasterizerState* Get(CullMode mode);

private:
    ComPtr<ID3D11RasterizerState> m_backSolid;
    ComPtr<ID3D11RasterizerState> m_frontSolid;
    ComPtr<ID3D11RasterizerState> m_noneSolid;
    // «—ˆ“I‚É Wireframe ‚È‚Ç‚à’Ç‰Á‚µ‚â‚·‚¢
};