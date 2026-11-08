#pragma once
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <windows.h>
#include "colorGradingLut.h" // LUT_SIZE を共有する

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

// =====================================================================
// ImportedLut：画像編集ソフト（GIMP）で作った LUT の帯（PNG）を読み、Texture3D に詰め替えるクラス
//
//   帯の約束（ニュートラルな帯 lut_neutral.png と同じ）：
//     幅 N*N、高さ N（N=32 なら 1024x32）。左から青の値ごとに N 枚の板が並ぶ
//     板の中は 横＝赤（左→右で増える）、縦＝緑（上→下で増える）
//     → 画像の (x, y) = (b*N + r, g) の画素が、格子点 (r, g, b) の出力色
//
//   ColorGradingLut（式から CS で焼く表）とは別のテクスチャにする：
//     ・読み込んだ表は「元データそのもの」。作り直すことがないので IMMUTABLE で1回だけ作る
//     ・F9 で切り替えるときは、Tonemap の t2 にどちらの SRV をつなぐかを選ぶだけ
//
//   読むのは WIC で直接：生のバイト列を受け取るので、PNG の sRGB の印でリニアに直されることがない
//   （DirectXTK の CreateWICTextureFromFile だと、印があると _SRGB 形式になり表が狂う）
// =====================================================================
class ImportedLut {
public:
    static constexpr UINT LUT_SIZE = ColorGradingLut::LUT_SIZE; // Tonemap が b5 で受け取る lutSize と同じにする

    ImportedLut() = default;
    ~ImportedLut() = default;

    bool Initialize(ID3D11Device* pDevice, const wchar_t* pFileName) {
        if (!pDevice || !pFileName) return false;

        // 1. PNG → RGBA 8bit のバイト列（CPU のメモリ）
        std::vector<uint32_t> strip;
        UINT width = 0, height = 0;
        if (!LoadRGBA8(pFileName, strip, width, height)) {
            OutputDebugStringW(L"[ImportedLut] PNG を読めませんでした\n");
            return false;
        }

        // 帯の大きさが約束どおりか確かめる（切り出しを間違えると、別の色の位置を読んでしまう）
        const UINT N = LUT_SIZE;
        if (width != N * N || height != N) {
            OutputDebugStringW(L"[ImportedLut] 帯の大きさが N*N x N ではありません\n");
            return false;
        }

        // 2. 2D の帯 → 3D の並び（x=R が一番細かく、次に y=G、一番外が z=B）
        //    LutBakeCS が書く並び（id.x→R、id.y→G、id.z→B）と同じなので、Tonemap の読む側はそのまま
        std::vector<uint32_t> volume(N * N * N);
        for (UINT b = 0; b < N; ++b) {
            for (UINT g = 0; g < N; ++g) {
                for (UINT r = 0; r < N; ++r) {
                    const uint32_t texel = strip[g * width + (b * N + r)];
                    // アルファは使わないので不透明にそろえる（GIMP が透明を書き出しても影響しない）
                    volume[(b * N + g) * N + r] = texel | 0xFF000000u;
                }
            }
        }

        // 3. Texture3D：IMMUTABLE なので、作るときに中身を渡す（あとから書き換えられない）
        //    R8G8B8A8_UNORM（_SRGB なし）：PNG の 0〜255 を 0〜1 として、そのまま読む
        D3D11_TEXTURE3D_DESC td = {};
        td.Width = N;
        td.Height = N;
        td.Depth = N;
        td.MipLevels = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // 3D の初期データは「間隔」を2つ渡す
        //   SysMemPitch      ：1行（y が1つ進む）あたりのバイト数 = N 画素 x 4バイト
        //   SysMemSlicePitch ：1枚（z が1つ進む）あたりのバイト数 = N 行 x 1行のバイト数
        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = volume.data();
        init.SysMemPitch = N * sizeof(uint32_t);
        init.SysMemSlicePitch = N * N * sizeof(uint32_t);

        HRESULT hr = pDevice->CreateTexture3D(&td, &init, m_texture.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = pDevice->CreateShaderResourceView(m_texture.Get(), nullptr, m_srv.GetAddressOf());
        return SUCCEEDED(hr);
    }

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }

private:
    // WIC で画像を読み、RGBA 8bit（1画素 = uint32_t、メモリ上は R,G,B,A の順）に変換して返す
    static bool LoadRGBA8(const wchar_t* pFileName, std::vector<uint32_t>& outPixels, UINT& outWidth, UINT& outHeight) {
        // WIC は COM の部品。COM の初期化は WinMain で1回だけ行っている（ここでは初期化も後始末もしない）
        bool ok = false;
        {
            using Microsoft::WRL::ComPtr;
            ComPtr<IWICImagingFactory> factory;
            ComPtr<IWICBitmapDecoder> decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            ComPtr<IWICFormatConverter> converter;

            HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
            // デコーダー（PNG の圧縮を解く）→ 1枚目のフレーム
            if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(pFileName, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.GetAddressOf());
            if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
            // 形式の変換：RGB 24bit でも RGBA 32bit でも、同じ RGBA 8bit にそろえる（色の値そのものは変えない）
            if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(converter.GetAddressOf());
            if (SUCCEEDED(hr)) hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
            if (SUCCEEDED(hr)) hr = converter->GetSize(&outWidth, &outHeight);
            if (SUCCEEDED(hr)) {
                outPixels.resize(static_cast<size_t>(outWidth) * outHeight);
                const UINT stride = outWidth * sizeof(uint32_t);
                hr = converter->CopyPixels(nullptr, stride, stride * outHeight, reinterpret_cast<BYTE*>(outPixels.data()));
            }
            ok = SUCCEEDED(hr);
        }
        return ok;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture3D> m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
};
