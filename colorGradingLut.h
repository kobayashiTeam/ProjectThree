#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "graphicsCommon.h"
#include "shaderManager.h"

// =====================================================================
// ColorGradingLut：カラーグレーディングの 3D LUT（32x32x32）を持ち、CS で焼くクラス
//
//   焼く（Bake）：パラメータ → LutBakeCS.hlsl → Texture3D（UAV として書く）
//   読む       ：Tonemap.hlsl が t2 に SRV としてつなぎ、1ピクセルにつき1回だけ読む
//
//   焼くのはパラメータが変わったときだけ（Renderer が LUT のモードを切り替えたとき）。
//   式がどれだけ長くなっても、毎フレームの手間は「表を1回読む」だけで変わらない。
//
//   表の中身はガンマ補正後の空間（画像編集ソフトで作る LUT と同じ約束）。
//   ステップ2で PNG から作った表も、同じ Texture3D に入れれば読む側はそのまま使える
// =====================================================================
class ColorGradingLut {
public:
    // 表の一辺の格子数。Tonemap.hlsl には b5 の lutSize で、LutBakeCS.hlsl には b0 で渡す
    static constexpr UINT LUT_SIZE = 32;
    // 1グループの一辺のスレッド数。LutBakeCS.hlsl の THREAD_GROUP_SIZE と必ずそろえる
    static constexpr UINT THREAD_GROUP_SIZE = 8;

    // 人が決めるパラメータ（ここから CS に渡す値を作る）
    struct GradingParams {
        float temperature = 0.0f; // 色温度：＋で暖かく（赤寄り）、－で冷たく（青寄り）。目安 -1〜1
        float tint = 0.0f;        // 色かぶり：＋でマゼンタ寄り、－で緑寄り。目安 -1〜1
        float saturation = 1.0f;  // 0=モノクロ、1=そのまま
        float contrast = 1.0f;    // 1=そのまま
        DirectX::XMFLOAT3 colorFilter = { 1.0f, 1.0f, 1.0f };
    };

    // モードごとの見た目（プリセット）。Off は呼ばれない前提だが、念のため Neutral と同じ値を返す
    static GradingParams GetPreset(LutMode mode) {
        GradingParams p; // 既定値＝何もしない（Neutral）
        switch (mode) {
        case LutMode::Warm: // 夕方のような暖かい色。少しコントラストを上げ、彩度は控えめ
            p.temperature = 0.5f;
            p.tint = 0.05f;
            p.saturation = 0.9f;
            p.contrast = 1.15f;
            break;
        case LutMode::Cool: // 曇りや夜明けのような冷たい色
            p.temperature = -0.5f;
            p.tint = -0.05f;
            p.saturation = 0.85f;
            p.contrast = 1.1f;
            break;
        case LutMode::Sepia: // チェーンのセピアを LUT で作り直したもの：彩度0 ＋ 茶色のフィルター
            p.saturation = 0.0f;
            p.colorFilter = { 1.0f, 0.75f, 0.45f };
            p.contrast = 1.05f;
            break;
        case LutMode::Off:
        case LutMode::Neutral:
        default:
            break;
        }
        return p;
    }

    ColorGradingLut() = default;
    ~ColorGradingLut() = default;

    bool Initialize(ID3D11Device* pDevice) {
        if (!pDevice) return false;

        m_pCS = ShaderManager::GetInstance().GetCS(ShaderID::LutBakeCS);
        if (!m_pCS) return false;

        // 1. Texture3D：CS で書く（UAV）＋ PS で読む（SRV）
        //    R16G16B16A16_FLOAT：8bit だと格子の間を補間したときに段が出やすいので、16bit にしておく
        //    （32x32x32 x 8バイト = 256KB）
        D3D11_TEXTURE3D_DESC td = {};
        td.Width = LUT_SIZE;
        td.Height = LUT_SIZE;
        td.Depth = LUT_SIZE;
        td.MipLevels = 1; // 表なのでミップは要らない（読む側も SampleLevel で 0 を指定する）
        td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        HRESULT hr = pDevice->CreateTexture3D(&td, nullptr, m_texture.GetAddressOf());
        if (FAILED(hr)) return false;

        // 2. ビュー：desc に nullptr を渡すと「テクスチャ全体を、テクスチャと同じ形式で見る」既定のビューになる
        hr = pDevice->CreateShaderResourceView(m_texture.Get(), nullptr, m_srv.GetAddressOf());
        if (FAILED(hr)) return false;
        hr = pDevice->CreateUnorderedAccessView(m_texture.Get(), nullptr, m_uav.GetAddressOf());
        if (FAILED(hr)) return false;

        // 3. CS 用の定数バッファ（b0）
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(BakeCB);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        hr = pDevice->CreateBuffer(&cbd, nullptr, m_cb.GetAddressOf());
        return SUCCEEDED(hr);
    }

    // パラメータから表を焼き直す（変わったときだけ呼ぶ）
    void Bake(ID3D11DeviceContext* pContext, const GradingParams& params) {
        if (!pContext || !m_pCS) return;

        // 1. ホワイトバランス：色温度で赤と青を逆向きに、色かぶりで緑を動かす（簡易版）
        //    そのままだと全体の明るさも変わるので、輝度（Rec.709）が 1 になるように割っておく
        //    （本格的には URP のように LMS という「目の3種類の細胞」の空間で掛けるが、ここでは RGB で近似）
        const float k = 0.2f; // 1 あたりの効きの強さ
        float r = 1.0f + k * params.temperature;
        float g = 1.0f - k * params.tint;
        float b = 1.0f - k * params.temperature;
        const float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        r /= lum; g /= lum; b /= lum;

        BakeCB cb = {};
        cb.whiteBalance = { r, g, b };
        cb.saturation = params.saturation;
        cb.colorFilter = params.colorFilter;
        cb.contrast = params.contrast;
        cb.lutSize = LUT_SIZE;
        pContext->UpdateSubresource(m_cb.Get(), 0, nullptr, &cb, 0, 0);

        // 2. CS のスロットにつなぐ（表の SRV が PS の t2 に残っていると UAV と同時につなげないが、
        //    Tonemap の Unbind で毎フレーム外しているので、ここでは外れている前提）
        ID3D11Buffer* cbs[] = { m_cb.Get() };
        ID3D11UnorderedAccessView* uavs[] = { m_uav.Get() };
        pContext->CSSetShader(m_pCS, nullptr, 0);
        pContext->CSSetConstantBuffers(0, 1, cbs);
        pContext->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        // 3. 3次元の Dispatch：一辺 32 ÷ 8 = 4 グループ → 4x4x4 = 64 グループ、合計 32768 スレッド
        const UINT groups = (LUT_SIZE + THREAD_GROUP_SIZE - 1) / THREAD_GROUP_SIZE;
        pContext->Dispatch(groups, groups, groups);

        // 4. 後片付け：UAV を外さないと、この後 PS の t2 に SRV としてつないだときに外されてしまう
        ID3D11UnorderedAccessView* nullUAV = nullptr;
        pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
        pContext->CSSetShader(nullptr, nullptr, 0);
    }

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

private:
    // LutBakeCS.hlsl の cbuffer LutBakeCB（b0）と同じ並び
    struct BakeCB {
        DirectX::XMFLOAT3 whiteBalance;
        float saturation;
        DirectX::XMFLOAT3 colorFilter;
        float contrast;
        UINT lutSize;
        float padding[3];
    };
    static_assert(sizeof(BakeCB) % 16 == 0, "cbuffer size must be a multiple of 16 bytes");

    ID3D11ComputeShader* m_pCS = nullptr; // 所有は ShaderManager
    ComPtr<ID3D11Texture3D> m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    ComPtr<ID3D11UnorderedAccessView> m_uav;
    ComPtr<ID3D11Buffer> m_cb;
};
