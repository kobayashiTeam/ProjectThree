#pragma once
#include <d3d11.h>
#include <wrl/client.h>

class TextRenderer;

// =========================================================
// GPUプロファイラ（タイムスタンプクエリ）
//
// 考え方：
//   CPUは注文票（コマンド）を積むだけで、GPUは後から自分のペースで処理する。
//   なのでCPU側の時計（QueryPerformanceCounter）ではGPUの仕事時間は測れない。
//   → 注文票の列に「ここで時刻を書いて」という票（タイムスタンプ）を差し込み、
//     GPU自身の時計で区間の前後を記録してもらう。
//
// 使い方（1フレーム）：
//   BeginFrame()                  … 外側のDisjointを開始＋フレーム先頭の時刻
//     Begin(区間) … End(区間)      … 測りたいパスを挟む（何区間でも）
//   EndFrame()                    … フレーム末尾の時刻＋Disjointを終了
//
// 結果はすぐ読まない：
//   GPUは数フレーム遅れて動いているので、直後に読むとCPUが完了待ちで止まる。
//   クエリを SLOT_COUNT 組用意して順番に使い回し、「SLOT_COUNT フレーム前」の結果を読む。
// =========================================================
class GpuProfiler
{
public:
    // 測る区間。増やすときはここと s_sectionNames（.cpp）の両方に足す
    enum class Section
    {
        ShadowDir,    // 旧・方向光シャドウマップ
        ShadowCSM,    // カスケードシャドウ（3段）
        ShadowPoint,  // 点光源キューブシャドウ（GSで6面）
        GBuffer,      // G-Buffer書き込み（DeferredOpaque）
        SSAO,         // SSAO生成＋ブラー
        Lighting,     // Deferredライティング
        Forward,      // フォワード不透明・インスタンシング・スカイ・半透明＋MSAAのResolve
        Bloom,        // HDR段：Bloom（縮小／拡大）
        Post,         // Tonemap＋LDRチェーン＋最終ブリット
        Count
    };

    GpuProfiler() = default;
    ~GpuProfiler() = default;

    bool Initialize(ID3D11Device* device);

    void BeginFrame(ID3D11DeviceContext* ctx);
    void EndFrame(ID3D11DeviceContext* ctx);

    void Begin(ID3D11DeviceContext* ctx, Section s);
    void End(ID3D11DeviceContext* ctx, Section s);

    // 画面右上に各区間のms（平均）を表示する。TextRendererのBegin/Endの間で呼ぶ
    void DrawOverlay(TextRenderer* text) const;

    void ToggleVisible() { m_visible = !m_visible; }
    bool IsVisible() const { return m_visible; }

private:
    // 古いスロットの結果を回収し、平均用に積算する
    void CollectSlot(ID3D11DeviceContext* ctx, int slot);

private:
    // CPUは最大3フレーム先行できる（DXGIの既定）。＋1の余裕を持たせて4組
    static constexpr int SLOT_COUNT = 4;
    static constexpr int SECTION_COUNT = static_cast<int>(Section::Count);
    // 表示の数字がちらつかないよう、このフレーム数ぶん平均してから表示を更新する
    static constexpr int AVERAGE_FRAMES = 30;

    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    struct Slot
    {
        ComPtr<ID3D11Query> disjoint;                 // 外側：周波数と「信用できるか」を教えてくれる
        ComPtr<ID3D11Query> frameBegin;
        ComPtr<ID3D11Query> frameEnd;
        ComPtr<ID3D11Query> begin[SECTION_COUNT];
        ComPtr<ID3D11Query> end[SECTION_COUNT];
        bool sectionIssued[SECTION_COUNT] = {};       // このフレームで実際に測った区間か
        bool frameIssued = false;                     // このスロットに結果が入っているか
    };

    Slot m_slots[SLOT_COUNT];
    int  m_current = 0;          // 今フレームで使っているスロット
    bool m_inFrame = false;

    // 平均の積算
    double m_accumMs[SECTION_COUNT] = {};
    int    m_accumCount[SECTION_COUNT] = {};
    double m_accumTotalMs = 0.0;
    int    m_accumFrames = 0;

    // 表示用（AVERAGE_FRAMESごとに更新）。負の値＝そのフレーム群で一度も測っていない
    float m_displayMs[SECTION_COUNT] = {};
    float m_displayTotalMs = 0.0f;

    bool m_visible = true;
};
