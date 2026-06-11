// Graphics.h
#pragma once
#include <d3d11.h>
#include <wrl/client.h>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class Graphics
{
public:
    Graphics();
    ~Graphics();

    bool Initialize(HWND hWnd, int width, int height);
    void Finalize();           // 後で実装
    void BeginScene(float r, float g, float b, float a);
    void EndScene();

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }

    // 一時的に公開メソッドを追加（後でRendererに移す）
    ID3D11DepthStencilState* GetDefaultDepthStencilState() const {
        return m_defaultDepthStencilState.Get();
    }

private:
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    ComPtr<ID3D11Texture2D> m_depthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    ComPtr<ID3D11DepthStencilState> m_defaultDepthStencilState;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
};