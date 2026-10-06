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

    // ===== シャドウ用 =====
    // 深度バイアスはラスタライザーが「書き込む深度」に足される量：
    //   bias = DepthBias * r + SlopeScaledDepthBias * MaxDepthSlope （上限 DepthBiasClamp）
    //   MaxDepthSlope ＝ その三角形が1ピクセル進むと深度がどれだけ変わるか（ライトに対して斜めな面ほど大きい）
    // ・DepthBias（定数項）は0：シャドウマップはD32_FLOATで、定数項の単位rが深度の値によって変わり扱いにくいため。
    //   定数ぶんのずらしはこれまでどおりシェーダー側（旧マップの0.005、CSMのcascadeBias）が受け持つ
    // ・Slopeは「傾いた面ほど大きく」ずらす担当。シェーダー側の定数では、傾いた面に合わせると平らな面で浮き（ピーターパン）、
    //   平らな面に合わせると傾いた面でアクネが出る、という板挟みになる。その片方をハードウェアに任せる
    // ・Clampは、ほぼ真横を向いた面（傾きが極端に大きい）で深度が飛びすぎないための上限（NDCの深度で0.005）
    // ・カリングはBack：閉じたメッシュなら、ライトから見て一番手前の深度は表面側で決まるので、Noneと結果は同じで裏面の分だけ軽い
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable = TRUE;
        desc.DepthBias = 0;
        desc.SlopeScaledDepthBias = 1.5f;
        desc.DepthBiasClamp = 0.005f;
        device->CreateRasterizerState(&desc, &m_shadowSolid);
    }

    return m_backSolid && m_frontSolid && m_noneSolid && m_shadowSolid;
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

void RasterizerStates::BindShadow(ID3D11DeviceContext* ctx)
{
    ctx->RSSetState(m_shadowSolid.Get());
}
