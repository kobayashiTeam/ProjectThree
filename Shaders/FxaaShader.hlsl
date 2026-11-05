// =====================================================================
// FXAA（Fast Approximate Anti-Aliasing）
//
//   遅延レンダリングでは、G-BufferがMSAAなし（1ピクセルに1つの値）なので、
//   ライティングの結果をMSAAのRTに書いても4サンプルとも同じ値になり、Resolveしてもジャギーが残る。
//   FXAAは G-Buffer も深度も見ず、「最後の絵」だけを見て輪郭を探し、後からぼかす。
//   なので、遅延・フォワードどちらで描いた物の輪郭にも同じように効く。
//
//   置き場所：Tonemapの直後（LDRの段の最初）。
//     ・HDRのうちだと明るさが1.0を超えて、輪郭の判定がぶれる
//     ・シャープなど「輪郭を強める」ポストプロセスより前に、ジャギーを消しておきたい
//
//   明るさ（luma）は「ガンマ補正後に近い値」で比べる（物差しだけ直す）。
//     ここに来る色はまだリニア（ガンマ補正は最後の ScreenBlit）。リニアのままだと
//     暗い所の差が小さく見えて輪郭を見逃しやすいので、平方根で人の目の感じ方に近づける。
//     出力する色はリニアのまま（直して出すと、ScreenBlitでガンマが2回かかる）。
//
//   流れ（1ピクセルごと）：
//     1. 上下左右の明るさの差が小さければ「輪郭ではない」→ そのまま返す
//     2. 輪郭が横向きか縦向きかを決める
//     3. 輪郭の線に沿って両側へ歩き、輪郭の端を探す
//     4. 端までの距離から「どれだけずらして読むか」を決める（端に近いほど大きくずらす）
//     5. 1ピクセルより細い物（サブピクセル）用のずらし量と比べて、大きい方を使う
//     6. ずらした位置をバイリニアで読む ＝ 隣のピクセルと混ざって、段がなめらかになる
// =====================================================================

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

Texture2D sceneTexture : register(t0); // Tonemap後のLDR（リニア）
SamplerState linearSampler : register(s0); // LINEAR・CLAMP（PostProcess::Render がつなぐ）

// C++側の FxaaPostProcess::PerEffectCB と同じ並びにする
cbuffer PerEffectCB : register(b2)
{
    int g_EdgeView; // 0=通常のFXAA、1=輪郭として扱ったピクセルを色で表示（デバッグ）
    float3 g_Padding;
};

// ---------------------------------------------------------------------
// 調整用の定数（FXAA 3.11 の Quality 版の既定値に近いもの）
// ---------------------------------------------------------------------
// 明るさの差がこれより小さければ輪郭ではない（暗い所のノイズを拾わないための下限）
static const float EDGE_THRESHOLD_MIN = 0.0312;
// 周りで一番明るい値に対して、この割合より差が小さければ輪郭ではない
static const float EDGE_THRESHOLD_MAX = 0.125;
// 輪郭に沿って歩く回数と、1回ごとの歩幅（ピクセル単位）。後半ほど大股にして遠くまで探す
static const int ITERATIONS = 12;
static const float QUALITY[12] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };
// サブピクセル（1ピクセルより細い物）をどれだけぼかすか。0で無効、1で最大
static const float SUBPIXEL_QUALITY = 0.75;

VS_OUTPUT VS(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Position = float4(input.Position, 1.0);
    output.TexCoord = input.TexCoord;
    return output;
}

// 輪郭を探すための「物差し」：色 → 明るさ（ガンマ補正後に近い値）
float Luma(float3 rgb)
{
    return sqrt(dot(saturate(rgb), float3(0.299, 0.587, 0.114)));
}

float LumaAt(float2 uv)
{
    // ループの中で読むので、ミップの自動選択（微分）が要らない SampleLevel を使う
    return Luma(sceneTexture.SampleLevel(linearSampler, uv, 0).rgb);
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    uint w, h;
    sceneTexture.GetDimensions(w, h);
    const float2 texel = 1.0 / float2(w, h); // 1ピクセルのUV上の大きさ

    const float2 uv = input.TexCoord;
    const float4 colorCenter = sceneTexture.SampleLevel(linearSampler, uv, 0);

    // UVは下向きが +v。N=上、S=下、W=左、E=右
    const float lumaC = Luma(colorCenter.rgb);
    const float lumaN = LumaAt(uv + float2(0.0, -texel.y));
    const float lumaS = LumaAt(uv + float2(0.0, texel.y));
    const float lumaW = LumaAt(uv + float2(-texel.x, 0.0));
    const float lumaE = LumaAt(uv + float2(texel.x, 0.0));

    // -----------------------------------------------------------------
    // 1. 輪郭かどうか：上下左右と自分の中で、明るさの幅（最大－最小）を見る
    // -----------------------------------------------------------------
    const float lumaMin = min(lumaC, min(min(lumaN, lumaS), min(lumaW, lumaE)));
    const float lumaMax = max(lumaC, max(max(lumaN, lumaS), max(lumaW, lumaE)));
    const float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
    {
        // 輪郭ではない → 何もしない（画面の大半はここで終わるので軽い）
        if (g_EdgeView != 0)
            return float4(colorCenter.rgb * 0.25, colorCenter.a); // デバッグ：輪郭でない所は暗く
        return colorCenter;
    }

    // -----------------------------------------------------------------
    // 2. 輪郭の向き：横向きの線か、縦向きの線か
    //    斜めの4つも使って、縦方向の変化の強さと横方向の変化の強さを比べる
    // -----------------------------------------------------------------
    const float lumaNW = LumaAt(uv + float2(-texel.x, -texel.y));
    const float lumaNE = LumaAt(uv + float2(texel.x, -texel.y));
    const float lumaSW = LumaAt(uv + float2(-texel.x, texel.y));
    const float lumaSE = LumaAt(uv + float2(texel.x, texel.y));

    const float lumaNS = lumaN + lumaS;
    const float lumaWE = lumaW + lumaE;
    const float lumaWCorners = lumaNW + lumaSW;
    const float lumaECorners = lumaNE + lumaSE;
    const float lumaNCorners = lumaNW + lumaNE;
    const float lumaSCorners = lumaSW + lumaSE;

    // 縦方向（上→自分→下）の明るさの曲がり具合 ＝ 横向きの線がある強さ
    const float edgeHorizontal =
        abs(-2.0 * lumaW + lumaWCorners) +
        abs(-2.0 * lumaC + lumaNS) * 2.0 +
        abs(-2.0 * lumaE + lumaECorners);
    // 横方向（左→自分→右）の明るさの曲がり具合 ＝ 縦向きの線がある強さ
    const float edgeVertical =
        abs(-2.0 * lumaN + lumaNCorners) +
        abs(-2.0 * lumaC + lumaWE) * 2.0 +
        abs(-2.0 * lumaS + lumaSCorners);

    const bool isHorizontal = (edgeHorizontal >= edgeVertical);

    // -----------------------------------------------------------------
    // 線のどちら側が「向こう側」か：変化が大きい方（上か下／左か右）
    // -----------------------------------------------------------------
    const float luma1 = isHorizontal ? lumaN : lumaW; // 負の側（上／左）
    const float luma2 = isHorizontal ? lumaS : lumaE; // 正の側（下／右）
    const float gradient1 = luma1 - lumaC;
    const float gradient2 = luma2 - lumaC;
    const bool is1Steepest = abs(gradient1) >= abs(gradient2);

    // 端の判定に使う「差の大きさ」。線をまたぐ差の1/4を超えたら、線が途切れた（端に着いた）とみなす
    const float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    // 線をまたぐ向きの1ピクセル（向こう側へ進む向きに符号を合わせる）
    float stepLength = isHorizontal ? texel.y : texel.x;
    float lumaLocalAverage;
    if (is1Steepest)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaC);
    }
    else
    {
        lumaLocalAverage = 0.5 * (luma2 + lumaC);
    }

    // 自分と向こう側のちょうど境目（半ピクセルずらした所）から歩き始める
    float2 currentUv = uv;
    if (isHorizontal)
        currentUv.y += stepLength * 0.5;
    else
        currentUv.x += stepLength * 0.5;

    // -----------------------------------------------------------------
    // 3. 線に沿って両側へ歩き、線の端を探す
    //    境目をバイリニアで読むと「自分と向こう側の平均」が読める。
    //    その値が lumaLocalAverage から大きく外れたら、線が途切れた＝端
    // -----------------------------------------------------------------
    const float2 offset = isHorizontal ? float2(texel.x, 0.0) : float2(0.0, texel.y);
    float2 uv1 = currentUv - offset * QUALITY[0];
    float2 uv2 = currentUv + offset * QUALITY[0];

    float lumaEnd1 = LumaAt(uv1) - lumaLocalAverage;
    float lumaEnd2 = LumaAt(uv2) - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;

    if (!reached1)
        uv1 -= offset * QUALITY[1];
    if (!reached2)
        uv2 += offset * QUALITY[1];

    if (!reachedBoth)
    {
        [loop]
        for (int i = 2; i < ITERATIONS; ++i)
        {
            if (!reached1)
                lumaEnd1 = LumaAt(uv1) - lumaLocalAverage;
            if (!reached2)
                lumaEnd2 = LumaAt(uv2) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;

            if (!reached1)
                uv1 -= offset * QUALITY[i];
            if (!reached2)
                uv2 += offset * QUALITY[i];

            if (reachedBoth)
                break;
        }
    }

    // -----------------------------------------------------------------
    // 4. 端までの距離 → ずらし量
    //    線の長さの真ん中にいるピクセルはずらさず（0）、端に近いほど大きく（最大0.5）ずらす。
    //    これで階段の1段（長い水平線の端で1ピクセル落ちる形）が、坂のようになめらかになる
    // -----------------------------------------------------------------
    const float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    const float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    const bool isDirection1 = distance1 < distance2;
    const float distanceFinal = min(distance1, distance2);
    const float edgeLength = distance1 + distance2;

    const float pixelOffset = -distanceFinal / edgeLength + 0.5;

    // 近い方の端で、明るさの変わり方が自分と食い違っていないか（食い違うなら、ずらさない）
    const bool isLumaCenterSmaller = lumaC < lumaLocalAverage;
    const bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;

    // -----------------------------------------------------------------
    // 5. サブピクセル：周り3x3の平均と自分の差が大きい ＝ 1ピクセルより細い物（細い線・点）
    //    線の端を探す方法では拾えないので、別のずらし量を作って大きい方を使う
    // -----------------------------------------------------------------
    const float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaNS + lumaWE) + lumaWCorners + lumaECorners);
    const float subPixelOffset1 = saturate(abs(lumaAverage - lumaC) / lumaRange);
    const float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1; // smoothstep の形
    const float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
    finalOffset = max(finalOffset, subPixelOffsetFinal);

    // -----------------------------------------------------------------
    // 6. 線をまたぐ向きにずらして読む（バイリニアで隣と混ざる）
    // -----------------------------------------------------------------
    float2 finalUv = uv;
    if (isHorizontal)
        finalUv.y += finalOffset * stepLength;
    else
        finalUv.x += finalOffset * stepLength;

    const float3 finalColor = sceneTexture.SampleLevel(linearSampler, finalUv, 0).rgb;

    if (g_EdgeView != 0)
    {
        // デバッグ：横向きの線は赤、縦向きの線は青。ずらし量が大きいほど明るい
        const float3 edgeColor = isHorizontal ? float3(1.0, 0.25, 0.15) : float3(0.15, 0.55, 1.0);
        return float4(edgeColor * saturate(0.35 + finalOffset * 1.3), colorCenter.a);
    }

    return float4(finalColor, colorCenter.a);
}
