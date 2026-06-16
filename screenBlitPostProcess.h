#pragma once
#include "PostProcess.h"

class ScreenBlitPostProcess : public PostProcess
{
public:
    ScreenBlitPostProcess() = default;
    ~ScreenBlitPostProcess() override = default;

    // 初期化と描画は基底クラス（PostProcess）の機能をそのまま利用します
    // 特殊な定数バッファの割り当てが不要なため、オーバーライド（上書き）は不要です
};