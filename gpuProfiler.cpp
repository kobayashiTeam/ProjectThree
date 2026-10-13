#include "gpuProfiler.h"
#include "textRenderer.h"
#include <cstdio>

namespace
{
    // Section と同じ順番で並べる
    const wchar_t* s_sectionNames[] =
    {
        L"ShadowDir",
        L"ShadowCSM",
        L"ShadowPoint",
        L"GBuffer",
        L"SSAO",
        L"Lighting",
        L"Forward",
        L"Bloom",
        L"Post",
    };
    static_assert(sizeof(s_sectionNames) / sizeof(s_sectionNames[0]) ==
        static_cast<size_t>(GpuProfiler::Section::Count), "s_sectionNamesとSectionの数が合っていない");

    // 大きな数を短く書く（例：950 / 12.3k / 1.25M）。列の幅をそろえるため
    void FormatCount(wchar_t* buf, size_t bufCount, double v)
    {
        if (v < 10000.0)          swprintf_s(buf, bufCount, L"%.0f", v);
        else if (v < 1000000.0)   swprintf_s(buf, bufCount, L"%.1fk", v / 1000.0);
        else                      swprintf_s(buf, bufCount, L"%.2fM", v / 1000000.0);
    }

    // 右端を rightX にそろえて描く（数字の列用）。MeasureString は scale 前の大きさを返す
    void DrawRight(TextRenderer* text, const wchar_t* s, float rightX, float y,
        float r, float g, float b, float scale)
    {
        float w = 0.0f, h = 0.0f;
        text->MeasureString(s, w, h);
        text->DrawString(s, rightX - w * scale, y, r, g, b, 1.0f, scale);
    }
}

bool GpuProfiler::Initialize(ID3D11Device* device)
{
    D3D11_QUERY_DESC disjointDesc = {};
    disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    D3D11_QUERY_DESC tsDesc = {};
    tsDesc.Query = D3D11_QUERY_TIMESTAMP;

    D3D11_QUERY_DESC statsDesc = {};
    statsDesc.Query = D3D11_QUERY_PIPELINE_STATISTICS;

    for (Slot& slot : m_slots)
    {
        if (FAILED(device->CreateQuery(&disjointDesc, &slot.disjoint))) return false;
        if (FAILED(device->CreateQuery(&tsDesc, &slot.frameBegin))) return false;
        if (FAILED(device->CreateQuery(&tsDesc, &slot.frameEnd))) return false;
        for (int i = 0; i < SECTION_COUNT; i++)
        {
            if (FAILED(device->CreateQuery(&tsDesc, &slot.begin[i]))) return false;
            if (FAILED(device->CreateQuery(&tsDesc, &slot.end[i]))) return false;
            if (FAILED(device->CreateQuery(&statsDesc, &slot.stats[i]))) return false;
        }
    }

    for (int i = 0; i < SECTION_COUNT; i++) m_displayMs[i] = -1.0f;
    return true;
}

void GpuProfiler::BeginFrame(ID3D11DeviceContext* ctx)
{
    // 次のスロットへ進む。そのスロットには SLOT_COUNT フレーム前の結果が入っているので、
    // 上書きする前に回収する
    m_current = (m_current + 1) % SLOT_COUNT;
    CollectSlot(ctx, m_current);

    Slot& slot = m_slots[m_current];
    for (bool& issued : slot.sectionIssued) issued = false;

    // Disjoint だけは Begin/End の2回呼ぶ（区間を囲む）。
    // タイムスタンプは「その瞬間の時刻」なので End だけを呼ぶ
    ctx->Begin(slot.disjoint.Get());
    ctx->End(slot.frameBegin.Get());
    m_inFrame = true;
}

void GpuProfiler::EndFrame(ID3D11DeviceContext* ctx)
{
    if (!m_inFrame) return;
    Slot& slot = m_slots[m_current];
    ctx->End(slot.frameEnd.Get());
    ctx->End(slot.disjoint.Get());
    slot.frameIssued = true;
    m_inFrame = false;
}

void GpuProfiler::Begin(ID3D11DeviceContext* ctx, Section s)
{
    if (!m_inFrame) return;
    Slot& slot = m_slots[m_current];
    int i = static_cast<int>(s);
    ctx->End(slot.begin[i].Get());      // 区間の開始時刻
    ctx->Begin(slot.stats[i].Get());    // ここから数え始める
}

void GpuProfiler::End(ID3D11DeviceContext* ctx, Section s)
{
    if (!m_inFrame) return;
    Slot& slot = m_slots[m_current];
    int i = static_cast<int>(s);
    ctx->End(slot.stats[i].Get());      // 数えるのはここまで
    ctx->End(slot.end[i].Get());        // 区間の終了時刻
    slot.sectionIssued[i] = true;
}

void GpuProfiler::CollectSlot(ID3D11DeviceContext* ctx, int slotIndex)
{
    Slot& slot = m_slots[slotIndex];
    if (!slot.frameIssued) return;      // 起動直後など、まだ一度も使っていない
    slot.frameIssued = false;

    // DONOTFLUSH：まだ終わっていなくても待たない（S_FALSEが返る）。
    // 待つとCPUがGPUの完了待ちで止まり、計測そのものが遅さの原因になる
    const UINT flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;

    // Disjointで捨てるフレームは、統計も一緒に捨てる（msと同じフレーム群で数字をそろえる）
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
    if (ctx->GetData(slot.disjoint.Get(), &dj, sizeof(dj), flags) != S_OK) return; // 未完了 → このフレームは捨てる
    if (dj.Disjoint) return;            // 計測中にクロックが変わった等 → 信用できないので捨てる

    // カウント差 → ミリ秒（Frequency は「1秒あたりのカウント数」）
    auto toMs = [&](UINT64 a, UINT64 b) {
        return static_cast<double>(b - a) / static_cast<double>(dj.Frequency) * 1000.0;
        };

    UINT64 fb = 0, fe = 0;
    if (ctx->GetData(slot.frameBegin.Get(), &fb, sizeof(fb), flags) != S_OK) return;
    if (ctx->GetData(slot.frameEnd.Get(), &fe, sizeof(fe), flags) != S_OK) return;
    m_accumTotalMs += toMs(fb, fe);

    for (int i = 0; i < SECTION_COUNT; i++)
    {
        if (!slot.sectionIssued[i]) continue;   // このフレームでは通らなかった区間（例：CSM無効）
        UINT64 b = 0, e = 0;
        D3D11_QUERY_DATA_PIPELINE_STATISTICS st = {};
        // 時刻と統計の3つがすべて取れた区間だけ積算する（片方だけ使うと別フレームの数字が混ざる）
        if (ctx->GetData(slot.begin[i].Get(), &b, sizeof(b), flags) != S_OK) continue;
        if (ctx->GetData(slot.end[i].Get(), &e, sizeof(e), flags) != S_OK) continue;
        if (ctx->GetData(slot.stats[i].Get(), &st, sizeof(st), flags) != S_OK) continue;

        m_accumMs[i] += toMs(b, e);
        m_accumVerts[i] += static_cast<double>(st.IAVertices);    // IAが読んだ頂点数（インデックスの数）
        m_accumTris[i] += static_cast<double>(st.IAPrimitives);  // IAが組み立てた三角形の数
        m_accumCount[i]++;
    }

    // 一定フレームたまったら平均を表示用に確定し、積算をリセット
    if (++m_accumFrames >= AVERAGE_FRAMES)
    {
        m_displayTotalMs = static_cast<float>(m_accumTotalMs / m_accumFrames);
        for (int i = 0; i < SECTION_COUNT; i++)
        {
            if (m_accumCount[i] > 0)
            {
                m_displayMs[i] = static_cast<float>(m_accumMs[i] / m_accumCount[i]);
                m_displayVerts[i] = m_accumVerts[i] / m_accumCount[i];
                m_displayTris[i] = m_accumTris[i] / m_accumCount[i];
            }
            else
            {
                m_displayMs[i] = -1.0f;
                m_displayVerts[i] = 0.0;
                m_displayTris[i] = 0.0;
            }
            m_accumMs[i] = 0.0;
            m_accumVerts[i] = 0.0;
            m_accumTris[i] = 0.0;
            m_accumCount[i] = 0;
        }
        m_accumTotalMs = 0.0;
        m_accumFrames = 0;
    }
}

void GpuProfiler::DrawOverlay(TextRenderer* text, float screenWidth) const
{
    if (!m_visible || !text) return;

    // 列の位置：名前は左端から、数字の列は右端をそろえる
    const float x = OverlayLeft(screenWidth);
    const float msR = x + 175.0f;     // ms 列の右端
    const float vertsR = x + 255.0f;     // Verts 列の右端
    const float trisR = x + 330.0f;     // Tris 列の右端（OVERLAY_WIDTH より少し内側）
    float y = 60.0f;
    const float lineH = 18.0f;
    const float scale = 0.6f;
    wchar_t buf[64];

    // 見出し
    text->DrawString(L"GPU  F3:Hide", x, y, 1.0f, 1.0f, 0.4f, 1.0f, scale);
    DrawRight(text, L"ms", msR, y, 1.0f, 1.0f, 0.4f, scale);
    DrawRight(text, L"Verts", vertsR, y, 1.0f, 1.0f, 0.4f, scale);
    DrawRight(text, L"Tris", trisR, y, 1.0f, 1.0f, 0.4f, scale);
    y += lineH;

    float  sumMs = 0.0f;
    double sumVerts = 0.0, sumTris = 0.0;
    for (int i = 0; i < SECTION_COUNT; i++)
    {
        const bool measured = (m_displayMs[i] >= 0.0f);
        // 測っていない区間は灰色
        const float c = measured ? 1.0f : 0.6f;
        text->DrawString(s_sectionNames[i], x, y, c, c, c, 1.0f, scale);

        if (!measured)
        {
            DrawRight(text, L"-", msR, y, c, c, c, scale);
            DrawRight(text, L"-", vertsR, y, c, c, c, scale);
            DrawRight(text, L"-", trisR, y, c, c, c, scale);
        }
        else
        {
            swprintf_s(buf, L"%.2f", m_displayMs[i]);
            DrawRight(text, buf, msR, y, c, c, c, scale);
            FormatCount(buf, 64, m_displayVerts[i]);
            DrawRight(text, buf, vertsR, y, c, c, c, scale);
            FormatCount(buf, 64, m_displayTris[i]);
            DrawRight(text, buf, trisR, y, c, c, c, scale);

            sumMs += m_displayMs[i];
            sumVerts += m_displayVerts[i];
            sumTris += m_displayTris[i];
        }
        y += lineH;
    }

    // Other＝Totalのうち、どの区間にも入っていない部分（クリア・UIテキスト・区間の隙間など）
    // 頂点数・三角形数は区間の中でしか数えていないので、Other には出さない
    float other = m_displayTotalMs - sumMs;
    if (other < 0.0f) other = 0.0f;
    text->DrawString(L"Other", x, y, 0.8f, 0.8f, 0.8f, 1.0f, scale);
    swprintf_s(buf, L"%.2f", other);
    DrawRight(text, buf, msR, y, 0.8f, 0.8f, 0.8f, scale);
    y += lineH;

    // Total の Verts/Tris は「全区間の合計」（UIテキストは含まない）
    text->DrawString(L"Total", x, y, 1.0f, 1.0f, 0.4f, 1.0f, scale);
    swprintf_s(buf, L"%.2f", m_displayTotalMs);
    DrawRight(text, buf, msR, y, 1.0f, 1.0f, 0.4f, scale);
    FormatCount(buf, 64, sumVerts);
    DrawRight(text, buf, vertsR, y, 1.0f, 1.0f, 0.4f, scale);
    FormatCount(buf, 64, sumTris);
    DrawRight(text, buf, trisR, y, 1.0f, 1.0f, 0.4f, scale);
}
