#include "bloomPass.h"
#include "renderTarget.h"
#include "shader.h"
#include "shaderManager.h"
#include "mesh.h"
#include "blendStates.h"

BloomPass::~BloomPass()
{
    for (RenderTarget* rt : m_mips) delete rt;
    m_mips.clear();
}

bool BloomPass::Initialize(ID3D11Device* device, UINT width, UINT height)
{
    m_sceneWidth = width;
    m_sceneHeight = height;

    m_downsampleShader = ShaderManager::GetInstance().GetShader(ShaderID::BloomDownsample);
    m_upsampleShader = ShaderManager::GetInstance().GetShader(ShaderID::BloomUpsample);
    if (!m_downsampleShader || !m_upsampleShader) return false;

    // ─── 段ごとのRTを作る（半分ずつ小さく） ───
    // フォーマットは R11G11B10_FLOAT：HDRの値を持てて、R16G16B16A16の半分のメモリで済む
    // （Bloomにはアルファも負の値も不要なので、この形式で足りる）
    UINT w = width;
    UINT h = height;
    for (int i = 0; i < MAX_MIPS; i++)
    {
        w = (w / 2 > 0) ? w / 2 : 1;
        h = (h / 2 > 0) ? h / 2 : 1;

        RenderTarget* rt = new RenderTarget();
        if (!rt->Initialize(device, w, h, DXGI_FORMAT_R11G11B10_FLOAT, false))
        {
            delete rt;
            return false;
        }
        m_mips.push_back(rt);

        if (w == 1 || h == 1) break; // これ以上小さくできない
    }

    // ─── サンプラー（LINEAR & CLAMP） ───
    // 縮小・拡大どちらも、テクセルの間を読んで4点を一度に平均させるためLINEARを使う
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateSamplerState(&sd, m_linearClamp.GetAddressOf()))) return false;

    // ─── 定数バッファ（b9） ───
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(BloomParamsCB); // 32バイト
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(device->CreateBuffer(&bd, nullptr, m_paramsCB.GetAddressOf()))) return false;

    return true;
}

void BloomPass::UpdateParams(ID3D11DeviceContext* ctx, UINT srcWidth, UINT srcHeight, bool isFirstPass)
{
    BloomParamsCB params = {};
    params.srcTexelSize = DirectX::XMFLOAT2(1.0f / srcWidth, 1.0f / srcHeight);
    params.threshold = m_threshold;
    params.knee = m_knee;
    params.isFirstPass = isFirstPass ? 1.0f : 0.0f;
    params.filterRadius = m_filterRadius;
    ctx->UpdateSubresource(m_paramsCB.Get(), 0, nullptr, &params, 0, 0);
}

ID3D11ShaderResourceView* BloomPass::Execute(
    ID3D11DeviceContext* ctx,
    ID3D11ShaderResourceView* sceneSRV,
    Mesh* fullscreenQuad,
    BlendStates* blendStates)
{
    if (m_mips.empty() || !sceneSRV) return nullptr;

    ID3D11Buffer* cb = m_paramsCB.Get();
    ctx->PSSetConstantBuffers(9, 1, &cb);
    ID3D11SamplerState* sampler = m_linearClamp.Get();
    ctx->PSSetSamplers(0, 1, &sampler);

    // 描き終わったら t0 を外す：次の工程で同じテクスチャをRTとしてバインドするとき、
    // 「読み込み中のテクスチャへの書き込み」になってD3Dに強制的に外される（警告が出る）のを防ぐ
    ID3D11ShaderResourceView* nullSRV = nullptr;

    // =============================
    // 1. 縮小：シーン → mip0 → mip1 → …
    // =============================
    blendStates->Bind(ctx, BlendMode::Opaque);
    m_downsampleShader->Bind(ctx);

    ID3D11ShaderResourceView* src = sceneSRV;
    UINT srcW = m_sceneWidth;
    UINT srcH = m_sceneHeight;

    for (size_t i = 0; i < m_mips.size(); i++)
    {
        RenderTarget* dst = m_mips[i];
        dst->Bind(ctx); // ビューポートもこの段のサイズになる（クリア不要：全画素を上書きするため）

        UpdateParams(ctx, srcW, srcH, i == 0);
        ctx->PSSetShaderResources(0, 1, &src);
        fullscreenQuad->Render(ctx);
        ctx->PSSetShaderResources(0, 1, &nullSRV);

        src = dst->GetSRV();
        srcW = dst->Width();
        srcH = dst->Height();
    }

    // =============================
    // 2. 拡大：一番小さい段から順に、1段大きい段へ加算で重ねる
    //    書き込み先には縮小時の絵が残っているので、そこへ「小さい段のにじみ」が足される
    // =============================
    blendStates->Bind(ctx, BlendMode::Additive);
    m_upsampleShader->Bind(ctx);

    for (size_t i = m_mips.size() - 1; i > 0; i--)
    {
        RenderTarget* smaller = m_mips[i];
        RenderTarget* larger = m_mips[i - 1];

        larger->Bind(ctx);

        UpdateParams(ctx, smaller->Width(), smaller->Height(), false);
        ID3D11ShaderResourceView* smallerSRV = smaller->GetSRV();
        ctx->PSSetShaderResources(0, 1, &smallerSRV);
        fullscreenQuad->Render(ctx);
        ctx->PSSetShaderResources(0, 1, &nullSRV);
    }

    // 後続の描画に加算ブレンドが残らないよう戻しておく
    blendStates->Bind(ctx, BlendMode::Opaque);

    return m_mips[0]->GetSRV();
}
