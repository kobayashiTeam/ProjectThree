#pragma once
#include <DirectXMath.h>

// シェーダーの Constant Buffer (b0) の物理的なレイアウトに完全一致させる構造体
struct PerFrameCB
{
    DirectX::XMMATRIX matView;
    DirectX::XMMATRIX matProjection;
    DirectX::XMFLOAT4 vLightPos;
    DirectX::XMFLOAT4 vLightColor;
    DirectX::XMFLOAT4 vEyePos;
    DirectX::XMFLOAT4 vAttenuation;
};

//ブレンドタイプの共通参照enum
enum class BlendMode
{
	Opaque,       // 不透明（None）
	AlphaBlend,   // 半透明
	Additive,     // 加算合成（エフェクト用）
	Count         // バッファの数（自動的に 3 になる）
};

// 描画の工程（ゲームのレンダリングステップ）
enum class RenderPass {
    Opaque,       // 1. 通常の不透明オブジェクト
    Outline,      // 2. 特殊処理：アウトライン
    Transparent,  // 3. 半透明オブジェクト
    Count
};

enum class ShaderID {
    Lit,
    Outline,
    UnLit,
    ScreenBlit,
    Monochromatic,
    // 今後増えるエフェクト（Scanline, Vignetteなど）をここに追加していくだけ！
    Count
};