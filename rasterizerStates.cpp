#include"rasterizerStates.h"

bool RasterizerStates::Initialize(ID3D11Device* device)
{
    auto CreateState = [&](D3D11_CULL_MODE cull) -> ComPtr<ID3D11RasterizerState> {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = cull;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable = TRUE;

        ComPtr<ID3D11RasterizerState> state;
        device->CreateRasterizerState(&desc, &state);
        return state;
        };

    m_backSolid = CreateState(D3D11_CULL_BACK);
    m_frontSolid = CreateState(D3D11_CULL_FRONT);
    m_noneSolid = CreateState(D3D11_CULL_NONE);

    return m_backSolid && m_frontSolid && m_noneSolid;
}

void RasterizerStates::Bind(ID3D11DeviceContext* ctx, CullMode mode)
{
    switch (mode)
    {
    case CullMode::Back:  ctx->RSSetState(m_backSolid.Get());  break;
    case CullMode::Front: ctx->RSSetState(m_frontSolid.Get()); break;
    case CullMode::None:  ctx->RSSetState(m_noneSolid.Get());  break;
    }
}