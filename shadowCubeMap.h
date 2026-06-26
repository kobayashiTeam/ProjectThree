#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <array>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class PointLight;  // forward

class ShadowCubeMap {
public:
    void Initialize(ID3D11Device* device, UINT size);
    void BeginRender(ID3D11DeviceContext* ctx);
    void EndRender(ID3D11DeviceContext* ctx);
    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

    // 6面分の行列をまとめて返す（GSのcbufferに渡す）
    std::array<DirectX::XMMATRIX, 6> GetLightSpaceMatrices() const;

    void SetLight(PointLight* light) { m_pLight = light; }
    float GetFarPlane() const { return m_farPlane; }  // PSで距離正規化に使う

private:
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11DepthStencilView>   m_dsv;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    const PointLight* m_pLight = nullptr;
    UINT  m_size = 1024;
    float m_farPlane = 100.0f;
};