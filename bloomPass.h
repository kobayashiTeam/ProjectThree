#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include "graphicsCommon.h"

class RenderTarget;
class Shader;
class Mesh;
class BlendStates;

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

// =====================================================================
// BloomPass：縮小／拡大の連鎖によるBloom（CoD:AW方式）
//
//   [縮小]  HDRシーン(1280x720) → 640x360 → 320x180 → … → 20x11
//           最初の1回でしきい値処理＋Karis平均、以降は13タップで縮小するだけ
//   [拡大]  20x11 → 40x22 → … → 640x360 へ、テントフィルタで拡大しながら加算で重ねる
//   結果：640x360 の段に、すべての段のにじみが積み重なった絵が残る（これを返す）
//
//   旧BloomBlurPass（フル解像度で横・縦ガウシアン1回）との違い：
//   ・小さい段ほど1テクセルが画面上で大きいので、少ない計算で遠くまでにじむ
//   ・段ごとのにじみが足し合わさるので、中心は鋭く・裾は広い自然な光り方になる
// =====================================================================
class BloomPass {
public:
    BloomPass() = default;
    ~BloomPass();

    bool Initialize(ID3D11Device* device, UINT width, UINT height);

    // sceneSRV：Resolve済みのHDRシーン
    // 戻り値：Bloomの最終結果（mip0＝半解像度）のSRV
    ID3D11ShaderResourceView* Execute(
        ID3D11DeviceContext* ctx,
        ID3D11ShaderResourceView* sceneSRV,
        Mesh* fullscreenQuad,
        BlendStates* blendStates);

    void SetThreshold(float v) { m_threshold = v; }
    void SetKnee(float v) { m_knee = v; }
    void SetFilterRadius(float v) { m_filterRadius = v; }

private:
    void UpdateParams(ID3D11DeviceContext* ctx, UINT srcWidth, UINT srcHeight, bool isFirstPass);

private:
    static constexpr int MAX_MIPS = 6;   // 1280x720なら 640x360 〜 20x11 の6段

    std::vector<RenderTarget*> m_mips;   // [0]が一番大きい（半解像度）
    UINT m_sceneWidth = 0;
    UINT m_sceneHeight = 0;

    Shader* m_downsampleShader = nullptr; // 管理はShaderManager
    Shader* m_upsampleShader = nullptr;
    ComPtr<ID3D11SamplerState> m_linearClamp;
    ComPtr<ID3D11Buffer> m_paramsCB;      // b9

    float m_threshold = 1.0f;     // 従来の「輝度1.0超えで発火」に合わせる
    float m_knee = 0.5f;          // しきい値の±0.5でなめらかに効き始める
    float m_filterRadius = 1.0f;  // 拡大時のテントの広がり（1.0＝1テクセル）
};
