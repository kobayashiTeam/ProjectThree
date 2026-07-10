#include "renderTarget.h"
#include<vector>

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
		float defaultColor[4] = { 0, 0, 0, 0 }; // 黒で初期化//w=0
		context->ClearRenderTargetView(m_rtv.Get(), defaultColor);
	}
	// 2. 深度バッファのクリア（存在する場合のみ）
	if (m_hasDepth) {
		context->ClearDepthStencilView(m_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}
}


bool RenderTarget::InitializeWithMSAA(ID3D11Device* device, uint32_t width, uint32_t height,
    DXGI_FORMAT colorFormat, uint32_t sampleCount, bool createDepth)
{
    m_width = width;
    m_height = height;
    m_hasDepth = createDepth;
    m_isMSAA = true; // MSAAとして初期化されたことを記録

    HRESULT hr;

    // ─── 共通処理：MSAAカラーバッファ（テクスチャ・RTV）の生成 ───
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = colorFormat;

    // ★ここが通常と違う！MSAAの設定
    textureDesc.SampleDesc.Count = sampleCount; // 4 や 8 など
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;

    // ★重要：MSAAテクスチャはそのままシェーダーで読めないため、
    // BIND_SHADER_RESOURCE は外し、RENDER_TARGET のみにします。
    textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;

    // 1. テクスチャ（カラー実体）の生成
    hr = device->CreateTexture2D(&textureDesc, nullptr, &m_texture);
    if (FAILED(hr)) return false;

    // 2. レンダーターゲットビュー（書き込み窓口）の作成
    // MSAA用の場合は、第2引数をnullptrにすれば自動的にマルチサンプル用として作成されます
    hr = device->CreateRenderTargetView(m_texture.Get(), nullptr, &m_rtv);
    if (FAILED(hr)) return false;

    // ★MSAAなので、シェーダーリソースビュー(SRV)は作成しません（m_srv = nullptrのまま）


    // ─── 条件付き処理：深度バッファが必要な場合のみ実行 ───
    if (!createDepth) {
        return true;
    }

    // 4. 深度バッファ（テクスチャ実体）の作成
    D3D11_TEXTURE2D_DESC descDepth = {};
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // ★カラーバッファとサンプル数を完全に一致させる！
    descDepth.SampleDesc.Count = sampleCount;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = device->CreateTexture2D(&descDepth, nullptr, &m_depthTexture);
    if (FAILED(hr)) return false;

    // 5. 深度ステンシルビュー（深度の窓口）の作成
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV = {};
    descDSV.Format = descDepth.Format;

    // ★通常は TEXTURE2D ですが、MSAAの場合は TEXTURE2DMS に指定する必要があります！
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;

    hr = device->CreateDepthStencilView(m_depthTexture.Get(), &descDSV, &m_dsv);
    if (FAILED(hr)) return false;

    return true;
}


// RenderTarget.cpp での実装
void RenderTarget::BindMultiple(
    ID3D11DeviceContext* context,
    uint32_t count,
    RenderTarget** targets,
    ID3D11DepthStencilView* dsv
) {
    if (count == 0 || !targets) return;

    // 1. RTVのポインタ配列を作る
    std::vector<ID3D11RenderTargetView*> rtvs(count);
    for (uint32_t i = 0; i < count; ++i) {
        rtvs[i] = targets[i]->m_rtv.Get();
    }

    // 2. パイプラインにまとめてバインド
    // &rtvs[0] で配列の先頭ポインタを渡す
    context->OMSetRenderTargets(count, &rtvs[0], dsv);

    // 3. ビューポートは「0番目のターゲット」のサイズに合わせる
    // (MRTの原則として、同時にバインドするRTのサイズは同じであるため)
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(targets[0]->m_width);
    vp.Height = static_cast<float>(targets[0]->m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
}

// RenderTarget.cpp
bool RenderTarget::InitializeDepthOnly(ID3D11Device* device, uint32_t width, uint32_t height,
    DXGI_FORMAT depthFormat)
{
    m_width = width;
    m_height = height;
    m_hasDepth = true;
    m_isMSAA = false;

    // 深度テクスチャだけ作成(m_textureやm_rtv, m_srvは一切生成しない)
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    //depthDesc.Format = depthFormat; // 例: DXGI_FORMAT_D32_FLOAT
    depthDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Typelessにする
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    //depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE; // 両方立てる

    HRESULT hr = device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.GetAddressOf());
    if (FAILED(hr)) return false;

	//DSVの作成
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    //dsvDesc.Format = depthFormat;
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    hr = device->CreateDepthStencilView(m_depthTexture.Get(), &dsvDesc, m_dsv.GetAddressOf());
    if (FAILED(hr)) return false;   // ★ここでは早期returnせず、失敗時のみfalseで抜ける

    // ----- SRV作成（★追記部分） -----
    // 
   // Lighting Passなどでこの深度をテクスチャとして読み込むために必要
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // SRV側はFloatとして解釈
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(m_depthTexture.Get(), &srvDesc, m_srv.GetAddressOf());
    if (FAILED(hr)) return false;

	return true;    
}