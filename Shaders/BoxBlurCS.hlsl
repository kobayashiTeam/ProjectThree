// =========================================================
// ボックスブラーのCS版（分離：横 → 縦 の2回の Dispatch で使う）
//
// 1グループ ＝ 1本の列の、連続した 256 画素
//   横の回：列 ＝ 画像の行（横に 256 画素）
//   縦の回：列 ＝ 画像の縦の列（縦に 256 画素）
// C++側は同じシェーダーを2回 Dispatch し、cbuffer の g_horizontal で向きだけを切り替える
//
// エントリポイントは2つ。違いは「近所をどこから読むか」だけ
//   CSMain         ：groupshared を使う版（近所を共有の配列から読む）
//   CSMainNoShared ：groupshared を使わない版（近所を毎回テクスチャから読む）
//   → 2つの ms の差が、そのまま groupshared の効果になる
// =========================================================

// BoxBlurCSPostProcess::GROUP_SIZE / MAX_RADIUS と必ずそろえる
#define GROUP_SIZE 256
#define MAX_RADIUS 16

// 読み込み：横の回なら直前の段の絵、縦の回なら横の結果（中間のRT）
Texture2D<float4> g_input : register(t0);
// 書き込み：横の回なら中間のRT、縦の回ならこの段の出力
RWTexture2D<float4> g_output : register(u0);

// C++の BoxBlurCSPostProcess::PerEffectCB と同じ並び
cbuffer BoxBlurCB : register(b0)
{
    uint2 g_size; // 画像の大きさ（横, 縦）。向きによらず、画像そのものの大きさ
    int g_radius; // 半径（1 ～ MAX_RADIUS。C++側で収めてから送る）
    uint g_horizontal; // 1 = 横の回、0 = 縦の回
};

// 共有の配列：担当の 256 画素 ＋ 左右の取り込み分（最大 MAX_RADIUS ずつ）
//   288 × 16バイト ＝ 4608 バイト。1グループの上限 32KB に十分収まる
//   中身はグループの処理が終わると消える。隣のグループからは見えない
groupshared float4 g_cache[GROUP_SIZE + 2 * MAX_RADIUS];

// 列の長さ（横の回なら画像の幅、縦の回なら画像の高さ）
int LineLength()
{
    return g_horizontal ? (int) g_size.x : (int) g_size.y;
}

// 「列の中の位置」と「何本目の列か」から、画像の座標を作る
// 縦の回は x と y を入れ替えるだけで、横の回と同じ書き方になる
int2 ToPixel(int along, int lineIndex)
{
    return g_horizontal ? int2(along, lineIndex) : int2(lineIndex, along);
}

// 列の外を読もうとしたら端の画素を読む（PS版のサンプラーの CLAMP と同じ扱い）
float4 LoadClamped(int along, int lineIndex)
{
    along = clamp(along, 0, LineLength() - 1);
    return g_input[ToPixel(along, lineIndex)];
}

// ---------------------------------------------------------
// groupshared を使う版
// ---------------------------------------------------------
// SV_GroupThreadID：グループの中での番号（0 ～ 255）。共有の配列の添字に使う
[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID, uint3 gtid : SV_GroupThreadID)
{
    int along = (int) dtid.x; // 列の中での位置
    int lineIndex = (int) dtid.y; // 何本目の列か（Dispatch の y は列の本数ちょうどなので、はみ出さない）
    int t = (int) gtid.x;
    int r = g_radius;

    // 1. 共有の配列に書く
    //    g_cache[k] には「グループの先頭 - r + k」の位置の画素が入る
    //    → 自分の画素は g_cache[t + r]
    g_cache[t + r] = LoadClamped(along, lineIndex);
    //    左端の r 本のスレッドは、左の取り込み分も読む（自分の位置 - r）
    if (t < r)
        g_cache[t] = LoadClamped(along - r, lineIndex);
    //    右端の r 本のスレッドは、右の取り込み分も読む（自分の位置 + r）
    if (t >= GROUP_SIZE - r)
        g_cache[t + 2 * r] = LoadClamped(along + r, lineIndex);

    // 2. グループの全員が書き終えるまで待つ
    //    これがないと、まだ書かれていない近所の欄を読んでしまう
    GroupMemoryBarrierWithGroupSync();

    // 3. はみ出したスレッドは、ここで止める
    //    Inversion のように最初に return してはいけない。
    //    ・はみ出したスレッドも、隣の画素の取り込み分を共有の配列に書く役目がある
    //    ・バリアは「グループの全員」が到着するのを待つので、途中で抜けるスレッドがいると成り立たない
    if (along >= LineLength())
        return;

    // 4. 近所（自分 ± r）を共有の配列から読んで平均する
    float4 sum = 0.0f;
    for (int k = -r; k <= r; ++k)
    {
        sum += g_cache[t + r + k];
    }
    g_output[ToPixel(along, lineIndex)] = sum / (float) (2 * r + 1);
}

// ---------------------------------------------------------
// groupshared を使わない版（比べるため）
// ---------------------------------------------------------
[numthreads(GROUP_SIZE, 1, 1)]
void CSMainNoShared(uint3 dtid : SV_DispatchThreadID)
{
    int along = (int) dtid.x;
    int lineIndex = (int) dtid.y;
    int r = g_radius;

    // バリアがないので、Inversion と同じく最初に止めてよい
    if (along >= LineLength())
        return;

    // 近所（自分 ± r）を、毎回テクスチャから読む
    float4 sum = 0.0f;
    for (int k = -r; k <= r; ++k)
    {
        sum += LoadClamped(along + k, lineIndex);
    }
    g_output[ToPixel(along, lineIndex)] = sum / (float) (2 * r + 1);
}
