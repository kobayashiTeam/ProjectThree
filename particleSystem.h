#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include "graphicsCommon.h"

class Shader;
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

// =========================================================
// GPUパーティクル（噴水）
//
// 粒の状態は1本の StructuredBuffer に置き、UAV と SRV の両方を作る
//   ・更新：CS が UAV（RWStructuredBuffer）として読み書きする。1スレッド＝1粒
//   ・描画：VS が SRV（StructuredBuffer）として SV_VertexID 番目を読む。頂点バッファは使わない
// 粒 i を読み書きするのはスレッド i だけ（他の粒を読まない）なので、1本のバッファに書き戻してよい
// （粒どうしが影響し合う動きを足すなら、読む用と書く用の2本に分ける必要がある）
//
// 寿命の扱い：数は固定。寿命が尽きた粒は、CS の中でその場で発生点に生まれ変わる
// 初期状態：全員の age を 0 にすると最初の1フレームで全員が同時に生まれるので、
//           age を負の値（生まれるまでの待ち時間）にばらして、生まれる時刻を散らす
//
// 比較用に、同じ計算を CPU で回して毎フレーム UpdateSubresource する経路も持つ
// =========================================================
class ParticleSystem
{
public:
    // HLSL側の struct Particle と同じ並び（ParticleUpdateCS.hlsl / ParticleDraw.hlsl の両方）
    // StructuredBuffer は cbuffer と違って16バイト境界の決まりはないが、
    // 16バイトの区切りにそろえておくと読み込みの単位と合って効率がよい
    struct Particle
    {
        DirectX::XMFLOAT3 position;
        float age;        // 生まれてからの秒数。負の値は「まだ生まれていない（待ち時間）」
        DirectX::XMFLOAT3 velocity;
        float lifetime;   // この粒の寿命（秒）
    };
    static_assert(sizeof(Particle) == 32, "Particle must match the HLSL struct (32 bytes)");

    // HLSL側の cbuffer ParticleUpdateCB（CS の b0）と同じ並び
    // CPU経路もこの値をそのまま使うので、GPUとCPUで同じ動きになる
    struct UpdateCB
    {
        DirectX::XMFLOAT3 emitterPos; // 発生点
        float dt;
        DirectX::XMFLOAT3 gravity;
        UINT  count;                  // 今動かす粒の数（はみ出したスレッドを止めるのに使う）
        UINT  frameSeed;              // 乱数の種。毎フレーム1ずつ増やす
        float floorY;                 // 床の高さ（ここで跳ね返る）
        float bounce;                 // 跳ね返りで残る縦の速さの割合
        float spread;                 // 横方向の初速の最大値
        float speedMin;               // 上向きの初速
        float speedMax;
        float lifeMin;                // 寿命
        float lifeMax;
    };
    static_assert(sizeof(UpdateCB) % 16 == 0, "cbuffer size must be a multiple of 16 bytes");

    // HLSL側の cbuffer ParticleDrawCB（GS/PS の b10）と同じ並び
    struct DrawCB
    {
        float size;      // 板の一辺の半分（ワールドの長さ）
        float intensity; // 色の明るさの倍率。数が多いときは下げる（加算なので重なると明るくなりすぎる）
        float pad[2];
    };
    static_assert(sizeof(DrawCB) % 16 == 0, "cbuffer size must be a multiple of 16 bytes");

    // 1グループのスレッド数。ParticleUpdateCS.hlsl の GROUP_SIZE と必ずそろえる
    static constexpr UINT GROUP_SIZE = 256;
    // 1回の Dispatch で x に渡せるグループ数は 65535 まで → 256 × 65535 ≒ 1677万個が上限
    static constexpr UINT MAX_DISPATCH_GROUPS = 65535;

    ParticleSystem() = default;
    ~ParticleSystem() = default;

    // maxCount 個ぶんのバッファを作り、初期状態を書き込む（CPU が中身に触るのは、ここと Reset だけ）
    bool Initialize(ID3D11Device* pDevice, UINT maxCount);

    // 毎フレームの設定（Scene が Update で呼ぶ）
    void SetDeltaTime(float dt) { m_updateParams.dt = dt; }
    void SetActiveCount(UINT count);
    void SetUpdateMode(ParticleUpdateMode mode);
    void SetDrawParams(float size, float intensity) { m_drawParams.size = size; m_drawParams.intensity = intensity; }

    // Renderer が Execute の中で呼ぶ
    void Update(ID3D11DeviceContext* pContext);                            // GPU経路：Dispatch ／ CPU経路：ループ＋UpdateSubresource
    void Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* perFrameCB);    // 点を Draw(count) → GS で板に広げる

    UINT GetActiveCount() const { return m_activeCount; }
    UINT GetMaxCount() const { return m_maxCount; }
    ParticleUpdateMode GetUpdateMode() const { return m_mode; }
    // CPU経路で、直近のフレームの「ループ＋UpdateSubresource の呼び出し」にかかったCPU時間（ms）
    float GetLastCpuUpdateMs() const { return m_lastCpuMs; }

private:
    void UpdateGPU(ID3D11DeviceContext* pContext);
    void UpdateCPU(ID3D11DeviceContext* pContext);
    // 初期状態（全員が発生点にいて、生まれるまでの待ち時間がばらばら）を作る
    void MakeInitialState(std::vector<Particle>& out) const;
    // GPUのバッファとCPUの配列を初期状態に戻す（方式を切り替えたときに、同じ条件から比べるため）
    void Reset(ID3D11DeviceContext* pContext);

private:
    UINT m_maxCount = 0;
    UINT m_activeCount = 0;
    ParticleUpdateMode m_mode = ParticleUpdateMode::GPU;
    bool m_resetRequested = false;

    UpdateCB m_updateParams = {};
    DrawCB   m_drawParams = {};

    // 粒の状態（1本に UAV と SRV の両方を作る）
    ComPtr<ID3D11Buffer> m_particleBuffer;
    ComPtr<ID3D11UnorderedAccessView> m_particleUAV; // CS の u0
    ComPtr<ID3D11ShaderResourceView>  m_particleSRV; // VS の t0

    ComPtr<ID3D11Buffer> m_updateCB;
    ComPtr<ID3D11Buffer> m_drawCB;

    // シェーダー（所有は ShaderManager）
    ID3D11ComputeShader* m_pUpdateCS = nullptr;
    Shader* m_pDrawShader = nullptr; // VS/PS（InputLayout なし）
    ID3D11GeometryShader* m_pDrawGS = nullptr;

    // CPU経路用の状態（GPU経路では使わない）
    std::vector<Particle> m_cpuParticles;
    float m_lastCpuMs = 0.0f;
};
