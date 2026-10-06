#pragma once
#include <d3d11.h>
#include <wrl/client.h>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class RasterizerStates {
public:
    enum class CullMode { Back, Front, None };

    bool Initialize(ID3D11Device* device);

    void Bind(ID3D11DeviceContext* ctx, CullMode mode);

    // シャドウマップ（方向光・CSM）を描くとき専用のステート
    // ハードウェアの深度バイアス（スロープスケール）で、面の傾きに応じて深度を奥へずらし、シャドウアクネを抑える
    // ※点光源キューブはPSでSV_Depthを自分で書くため、ハードウェアのバイアスが効かない。こちらは使わずBackを使う
    void BindShadow(ID3D11DeviceContext* ctx);

    ID3D11RasterizerState* Get(CullMode mode);

private:
    ComPtr<ID3D11RasterizerState> m_backSolid;
    ComPtr<ID3D11RasterizerState> m_frontSolid;
    ComPtr<ID3D11RasterizerState> m_noneSolid;
    ComPtr<ID3D11RasterizerState> m_shadowSolid;
};