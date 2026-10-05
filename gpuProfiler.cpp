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
}

bool GpuProfiler::Initialize(ID3D11Device* device)
{
    D3D11_QUERY_DESC disjointDesc = {};
    disjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    D3D11_QUERY_DESC tsDesc = {};
    tsDesc.Query = D3D11_QUERY_TIMESTAMP;

    for (Slot& slot : m_slots)
    {
        if (FAILED(device->CreateQuery(&disjointDesc, &slot.disjoint))) return false;
        if (FAILED(device->CreateQuery(&tsDesc, &slot.frameBegin))) return false;
        if (FAILED(device->CreateQuery(&tsDesc, &slot.frameEnd))) return false;
        for (int i = 0; i < SECTION_COUNT; i++)
        {
            if (FAILED(device->CreateQuery(&tsDesc, &slot.begin[i]))) return false;
            if (FAILED(device->CreateQuery(&tsDesc, &slot.end[i]))) return false;
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
    ctx->End(m_slots[m_current].begin[static_cast<int>(s)].Get());
}

void GpuProfiler::End(ID3D11DeviceContext* ctx, Section s)
{
    if (!m_inFrame) return;
    Slot& slot = m_slots[m_current];
    int i = static_cast<int>(s);
    ctx->End(slot.end[i].Get());
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
        if (ctx->GetData(slot.begin[i].Get(), &b, sizeof(b), flags) != S_OK) continue;
        if (ctx->GetData(slot.end[i].Get(), &e, sizeof(e), flags) != S_OK) continue;
        m_accumMs[i] += toMs(b, e);
        m_accumCount[i]++;
    }

    // 一定フレームたまったら平均を表示用に確定し、積算をリセット
    if (++m_accumFrames >= AVERAGE_FRAMES)
    {
        m_displayTotalMs = static_cast<float>(m_accumTotalMs / m_accumFrames);
        for (int i = 0; i < SECTION_COUNT; i++)
        {
            m_displayMs[i] = (m_accumCount[i] > 0)
                ? static_cast<float>(m_accumMs[i] / m_accumCount[i])
                : -1.0f;
            m_accumMs[i] = 0.0;
            m_accumCount[i] = 0;
        }
        m_accumTotalMs = 0.0;
        m_accumFrames = 0;
    }
}

void GpuProfiler::DrawOverlay(TextRenderer* text) const
{
    if (!m_visible || !text) return;

    const float x = 1030.0f;
    float y = 60.0f;
    const float lineH = 18.0f;
    const float scale = 0.6f;
    wchar_t buf[64];

    text->DrawString(L"GPU (ms)  F3:Hide", x, y, 1.0f, 1.0f, 0.4f, 1.0f, scale);
    y += lineH;

    float sum = 0.0f;
    for (int i = 0; i < SECTION_COUNT; i++)
    {
        if (m_displayMs[i] < 0.0f)
        {
            swprintf_s(buf, L"%-12s   -", s_sectionNames[i]);
            text->DrawString(buf, x, y, 0.6f, 0.6f, 0.6f, 1.0f, scale); // 測っていない区間は灰色
        }
        else
        {
            swprintf_s(buf, L"%-12s %5.2f", s_sectionNames[i], m_displayMs[i]);
            text->DrawString(buf, x, y, 1.0f, 1.0f, 1.0f, 1.0f, scale);
            sum += m_displayMs[i];
        }
        y += lineH;
    }

    // Other＝Totalのうち、どの区間にも入っていない部分（クリア・UIテキスト・区間の隙間など）
    float other = m_displayTotalMs - sum;
    if (other < 0.0f) other = 0.0f;
    swprintf_s(buf, L"%-12s %5.2f", L"Other", other);
    text->DrawString(buf, x, y, 0.8f, 0.8f, 0.8f, 1.0f, scale);
    y += lineH;

    swprintf_s(buf, L"%-12s %5.2f", L"Total", m_displayTotalMs);
    text->DrawString(buf, x, y, 1.0f, 1.0f, 0.4f, 1.0f, scale);
}
