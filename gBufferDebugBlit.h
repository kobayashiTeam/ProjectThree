#pragma once
#include "PostProcess.h"

// Scene3（遅延レンダリング）用：Gバッファ(Albedo/Normal/Depth)のSRVを
// 補正なしでそのままバックバッファに描くだけのシンプルなPostProcess。
// ScreenBlitPostProcess同様、初期化と描画は基底クラス（PostProcess）の機能をそのまま利用する。
class GBufferDebugBlit : public PostProcess
{
public:
    GBufferDebugBlit() = default;
    ~GBufferDebugBlit() override = default;
};
