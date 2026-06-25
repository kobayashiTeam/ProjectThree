#pragma once

#include<d3d11.h>
#include<DirectXMath.h>
#include<wrl/client.h>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

//forward
class Light;

class ShadowMap {
public:
    void Initialize(ID3D11Device* device, UINT size);
    void BeginRender(ID3D11DeviceContext* ctx);   // パス1の開始
    void EndRender(ID3D11DeviceContext* ctx);

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }    // パス2でシェーダに渡す
    DirectX::XMMATRIX GetLightSpaceMatrix() const;         // cbufferに渡す行列
    void setLight(Light* light) { m_pLight = light; }

private:
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11DepthStencilView>   m_dsv;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    const Light* m_pLight;  // 外から受け取る（所有しない）
    UINT m_size;
};