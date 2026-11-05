// =====================================================================
// カラーグレーディング用 3D LUT の焼き込み（コンピュートシェーダー）
//
//   32x32x32 の格子の1点を、1スレッドが受け持つ。
//   格子の位置＝入力の色（ガンマ補正後の 0〜1）。その色を式に通した結果を、その点に書く。
//   画面のピクセル数（約92万）ではなく、格子の数（32768）だけ式を計算すればよい。
//
//   表の入力・出力はどちらも「ガンマ補正後（pow 1/2.2）」の空間
//   （画像編集ソフトで作る LUT と同じ約束。ScreenBlit の pow 1/2.2 とそろえてある）
//   式の中では、混ぜ合わせ（ホワイトバランス・彩度）は光の量としてリニアで行い、
//   コントラストだけは見た目の明るさとしてガンマ側で行う
// =====================================================================

cbuffer LutBakeCB : register(b0)
{
    float3 g_WhiteBalance; // R・G・B に掛ける倍率（明るさが変わらないよう C++ 側で正規化済み）
    float g_Saturation; // 0=モノクロ、1=そのまま、1より大きいと鮮やかに
    float3 g_ColorFilter; // 全体に掛ける色（1,1,1 で何もしない）
    float g_Contrast; // 1=そのまま。中間の灰色を軸に、明暗の差を広げる／縮める
    uint g_LutSize; // 格子の数（一辺）。C++ の ColorGradingLut::LUT_SIZE
    float3 g_Padding;
};

RWTexture3D<float4> g_Lut : register(u0);

// コントラストの軸：リニアで 18% の灰色（写真の「中間の灰色」）をガンマ側に直した値
static const float CONTRAST_PIVOT = 0.4587; // pow(0.18, 1/2.2)

// 1グループの一辺のスレッド数。C++ の ColorGradingLut::THREAD_GROUP_SIZE と必ずそろえる
#define THREAD_GROUP_SIZE 8

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, THREAD_GROUP_SIZE)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // 格子の外のスレッドは何もしない（32 は 8 で割り切れるが、大きさを変えたときの保険）
    if (any(id >= g_LutSize))
        return;

    // 1. 格子の位置 → 入力の色（ガンマ側）。端の点がちょうど 0 と 1 になるように (N-1) で割る
    const float3 encoded = float3(id) / float(g_LutSize - 1);

    // 2. リニアに戻す（混ぜ合わせは光の量で行う）
    float3 c = pow(encoded, 2.2);

    // 3. ホワイトバランス：カメラのセンサーの段階で色かぶりを直すイメージなので、最初に掛ける
    c *= g_WhiteBalance;

    // 4. 彩度：明るさ（Rec.709 の輝度。リニアの値から作る Y）と元の色の間を補間する
    //    0 で灰色、1 でそのまま、1 を超えると灰色から遠ざかる＝鮮やかになる
    const float luminance = dot(c, float3(0.2126, 0.7152, 0.0722));
    c = lerp(luminance.xxx, c, g_Saturation);

    // 5. カラーフィルター：レンズの前に色ガラスを置くイメージ（セピアは 彩度0 ＋ 茶色のフィルター）
    c *= g_ColorFilter;

    // 6. コントラスト：見た目の明るさで扱いたいのでガンマ側に直してから、軸を中心に伸び縮みさせる
    float3 g = pow(saturate(c), 1.0 / 2.2);
    g = (g - CONTRAST_PIVOT) * g_Contrast + CONTRAST_PIVOT;

    // 7. 出力もガンマ側のまま書く（読む側の Tonemap.hlsl で pow 2.2 してリニアに戻す）
    g_Lut[id] = float4(saturate(g), 1.0);
}
