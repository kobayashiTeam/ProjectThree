#include "renderTarget.h"

bool RenderTarget::Initialize(ID3D11Device* device, uint32_t width, uint32_t height,
    DXGI_FORMAT colorFormat, bool createDepth)
{
    m_width = width;
    m_height = height;
    m_hasDepth = createDepth; // メンバ変数に状態を記録しておく（後でBindやResizeで便利）

    HRESULT hr;

    // ─── 共通処理：カラーバッファ（テクスチャ・RTV・SRV）の生成 ───
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = colorFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    // 1. テクスチャ（カラー実体）の生成
    hr = device->CreateTexture2D(&textureDesc, nullptr, &m_texture);
    if (FAILED(hr)) return false;

    // 2. レンダーターゲットビュー（書き込み窓口）の作成
    hr = device->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv);
    if (FAILED(hr)) return false;

    // 3. シェーダーリソースビュー（読み込み窓口）の作成
    hr = device->CreateShaderResourceView(m_texture.Get(), nullptr, &m_srv);
    if (FAILED(hr)) return false;


    // ─── 条件付き処理：深度バッファが必要な場合のみ実行 ───
    if (!createDepth) {
        return true; // 深度が不要（ポストプロセス用など）なら、ここで成功として終了！
    }

    // 4. 深度バッファ（テクスチャ実体）の作成
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = device->CreateTexture2D(&descDepth, nullptr, &m_depthTexture);
    if (FAILED(hr)) return false;

    // 5. 深度ステンシルビュー（深度の窓口）の作成
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    hr = device->CreateDepthStencilView(m_depthTexture.Get(), &descDSV, &m_dsv);
    if (FAILED(hr)) return false;

    m_hasDepth = true;
    return true;
}

void RenderTarget::Bind(ID3D11DeviceContext* context) {
    // m_hasDepth が false の場合、m_dsv.Get() は自動的に nullptr になる。
    // D3D11は第3引数の nullptr を「深度バッファなし」として正しく受け付ける。
    ID3D11DepthStencilView* dsv = m_hasDepth ? m_dsv.Get() : nullptr;

    // パイプラインにレンダーターゲット（色）と深度バッファ（窓口）を連結
    context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), dsv);

    // 【重要】以前お話しした通り、ビューポートもこのバッファのサイズに合わせる
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
}


void RenderTarget::Clear(ID3D11DeviceContext* context, const float* color) {
    //宣言時点でcolorにデフォルト引数があったとしても、こちらの本定義では
    //const float* colorで終わらせる

	// 1. カラーバッファのクリア
	if (color) {
		context->ClearRenderTargetView(m_rtv.Get(), color);
	}
	else {
		float defaultColor[4] = { 0, 0, 0, 1 }; // 黒で初期化
		context->ClearRenderTargetView(m_rtv.Get(), defaultColor);
	}
	// 2. 深度バッファのクリア（存在する場合のみ）
	if (m_hasDepth) {
		context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}