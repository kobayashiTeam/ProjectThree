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


    bool Initialize(ID3D11Device* device, uint32_t width, uint32_t height,
        DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM, bool createDepth = true);
    bool InitializeWithMSAA(ID3D11Device* device, uint32_t width, uint32_t height, 
        DXGI_FORMAT colorFormat= DXGI_FORMAT_R8G8B8A8_UNORM, uint32_t sampleCount=4, 
        bool createDepth=true);
    // 複数のRenderTargetで共有する深度バッファを生成する
    // カラーバッファは使用しないため、RTVは作成しない
    bool InitializeDepthOnly(ID3D11Device* device, uint32_t width, uint32_t height,
        DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT);
    // 複数のRenderTargetを同時にバインドするための静的関数
    static void BindMultiple(
        ID3D11DeviceContext* context,
        uint32_t count,
        RenderTarget** targets,
        ID3D11DepthStencilView* dsv = nullptr
    );

private:
	ComPtr<ID3D11Texture2D> m_texture;// カラーバッファ
    ComPtr<ID3D11RenderTargetView> m_rtv;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    ComPtr<ID3D11Texture2D> m_depthTexture;   // 深度付きの場合のバッファ
    ComPtr<ID3D11DepthStencilView> m_dsv;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_hasDepth = false;
    bool m_isMSAA = false; // MSAAかどうかを記録しておくと便利
};