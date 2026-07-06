#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class RenderTarget {
public:
	RenderTarget() = default;

    ~RenderTarget() = default;

    void Bind(ID3D11DeviceContext* context);
    void BindAsShaderResource(ID3D11DeviceContext* context, UINT slot = 0);
    void Clear(ID3D11DeviceContext* context, const float* color = nullptr);

    void Resize(ID3D11Device* device, uint32_t newWidth, uint32_t newHeight);

    ID3D11RenderTargetView* GetRTV() const { return m_rtv.Get(); }
    ID3D11DepthStencilView* GetDSV() const { return m_dsv.Get(); }
    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    ID3D11Texture2D* GetTexture() const { return m_texture.Get(); }

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

    //テスト：initialize
    // 1つのメソッドに統合。デフォルトでは深度バッファも作成する設定にする
    bool Initialize(ID3D11Device* device, uint32_t width, uint32_t height,
        DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM, bool createDepth = true);
    // ★新しく追加
    bool InitializeWithMSAA(ID3D11Device* device, uint32_t width, uint32_t height, 
        DXGI_FORMAT colorFormat= DXGI_FORMAT_R8G8B8A8_UNORM, uint32_t sampleCount=4, 
        bool createDepth=true);

private:
	ComPtr<ID3D11Texture2D> m_texture;// カラーバッファー
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    ComPtr<ID3D11Texture2D> m_depthTexture;   // 深度付きの場合のバッファ
    ComPtr<ID3D11DepthStencilView> m_dsv;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_hasDepth = false;
    bool m_isMSAA = false; // MSAAかどうかを記録しておくと便利
};