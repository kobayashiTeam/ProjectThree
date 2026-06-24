// Graphics.h
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

    // 他のクラスからDirectXオブジェクトを安全に利用するためのゲッター
    ID3D11Device* GetDevice() const { return m_pd3dDevice; }
    ID3D11DeviceContext* GetContext() const { return m_pImmediateContext; }

    //テスト
	void bindDefaultRenderTarget();
    //描画先カラーバッファを直接外部から欲しいときがある
    ID3D11Texture2D* getBackBufferTexture() {return pBackBuffer; }

private:
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pImmediateContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11Texture2D* pBackBuffer = nullptr;
    ID3D11RenderTargetView* m_pRenderTargetView = nullptr;
    //描画領域と被るようなオブジェクトは大体texture2dの型？
    ID3D11Texture2D* m_pDepthStencil = nullptr;
    ID3D11DepthStencilView* m_pDepthStencilView = nullptr;

    //テスト
public:
    
};