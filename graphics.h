#pragma once
#include <d3d11.h>

class Graphics
{
public:
    Graphics();
    ~Graphics();

    // 初期化と解放
    bool Initialize(HWND hWnd, int width, int height);
    void Finalize();

    // フレームの開始と終了（描画ループ用）
    void BeginScene(float r, float g, float b, float a);
    void EndScene();

    // DirectXリソースへアクセスするための取得関数
    ID3D11Device* GetDevice() const { return m_pd3dDevice; }
    ID3D11DeviceContext* GetContext() const { return m_pImmediateContext; }

	void bindDefaultRenderTarget();
    // 描画先カラーバッファを直接取得する場合に使用
    ID3D11Texture2D* getBackBufferTexture() {return pBackBuffer; }
    
    void ToggleVSync() { m_vsync = !m_vsync; }
    bool IsVSyncEnabled() const { return m_vsync; }

private:
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pImmediateContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11Texture2D* pBackBuffer = nullptr;
    ID3D11RenderTargetView* m_pRenderTargetView = nullptr;
    ID3D11Texture2D* m_pDepthStencil = nullptr;
    ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
    bool m_vsync = true;   // 起動時は今までどおりオン

};