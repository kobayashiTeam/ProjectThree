#include "particleSystem.h"
#include "shader.h"
#include "shaderManager.h"
#include <chrono>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// =========================================================
// CPU経路のための、HLSL（ParticleUpdateCS.hlsl）と同じ計算
// 乱数・生まれ変わり・1ステップの更新を、シェーダーと同じ手順で書く
// （浮動小数の細かい誤差は出るが、見た目は同じになる）
// =========================================================
namespace
{
    // PCGハッシュ：番号を混ぜて、ばらばらに見える32ビットの値にする
    uint32_t PcgHash(uint32_t v)
    {
        uint32_t state = v * 747796405u + 2891336453u;
        uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return (word >> 22u) ^ word;
    }

    // 上位24ビットを使って [0, 1) の小数にする
    float ToUnit(uint32_t h)
    {
        return static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
    }

    float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    // 粒 i を発生点に生まれ変わらせる（age = 0）
    ParticleSystem::Particle Spawn(uint32_t i, uint32_t seed, const ParticleSystem::UpdateCB& cb)
    {
        uint32_t h = PcgHash(i ^ PcgHash(seed));
        const float r0 = ToUnit(h); h = PcgHash(h);
        const float r1 = ToUnit(h); h = PcgHash(h);
        const float r2 = ToUnit(h); h = PcgHash(h);
        const float r3 = ToUnit(h);

        // 横の初速は円の中に一様に散らす（半径に sqrt を掛けないと中心に偏る）
        const float angle = r0 * 6.2831853f;
        const float radius = cb.spread * std::sqrt(r1);

        ParticleSystem::Particle p;
        p.position = cb.emitterPos;
        p.velocity = XMFLOAT3(std::cos(angle) * radius, Lerp(cb.speedMin, cb.speedMax, r2), std::sin(angle) * radius);
        p.age = 0.0f;
        p.lifetime = Lerp(cb.lifeMin, cb.lifeMax, r3);
        return p;
    }

    // 1粒を1フレームぶん進める。ParticleUpdateCS.hlsl の Simulate と同じ手順
    void Simulate(ParticleSystem::Particle& p, uint32_t i, const ParticleSystem::UpdateCB& cb)
    {
        p.age += cb.dt;
        if (p.age < 0.0f) return;                 // まだ生まれていない：発生点で待つ
        if (p.age >= p.lifetime) {                // 寿命が尽きた：その場で生まれ変わる
            p = Spawn(i, cb.frameSeed, cb);
            return;
        }

        p.velocity.x += cb.gravity.x * cb.dt;
        p.velocity.y += cb.gravity.y * cb.dt;
        p.velocity.z += cb.gravity.z * cb.dt;
        p.position.x += p.velocity.x * cb.dt;
        p.position.y += p.velocity.y * cb.dt;
        p.position.z += p.velocity.z * cb.dt;

        // 床で跳ね返る（縦は bounce の割合だけ残して反転、横は摩擦で 0.8 倍。0.8 はHLSLとそろえる）
        if (p.position.y < cb.floorY && p.velocity.y < 0.0f) {
            p.position.y = cb.floorY;
            p.velocity.y *= -cb.bounce;
            p.velocity.x *= 0.8f;
            p.velocity.z *= 0.8f;
        }
    }
}

bool ParticleSystem::Initialize(ID3D11Device* pDevice, UINT maxCount)
{
    if (!pDevice || maxCount == 0) return false;
    // 全員を1回の Dispatch で動かせる数まで
    if (maxCount > GROUP_SIZE * MAX_DISPATCH_GROUPS) return false;

    m_maxCount = maxCount;
    m_activeCount = maxCount;

    // シェーダーを取り出す
    ShaderManager& sm = ShaderManager::GetInstance();
    m_pUpdateCS = sm.GetCS(ShaderID::ParticleUpdateCS);
    m_pDrawShader = sm.GetShader(ShaderID::ParticleDraw);
    m_pDrawGS = sm.getGS(ShaderID::ParticleDrawGS);
    if (!m_pUpdateCS || !m_pDrawShader || !m_pDrawGS) return false;

    // 動きの既定値（噴水）。床の上面は y = -1.9（Scene12の床）
    m_updateParams.emitterPos = XMFLOAT3(0.0f, -1.9f, 4.0f);
    m_updateParams.dt = 0.016f;
    m_updateParams.gravity = XMFLOAT3(0.0f, -9.8f, 0.0f);
    m_updateParams.count = m_activeCount;
    m_updateParams.frameSeed = 1;            // 0 は初期状態を作るのに使うので、1から始める
    m_updateParams.floorY = -1.9f;
    m_updateParams.bounce = 0.35f;
    m_updateParams.spread = 1.6f;
    m_updateParams.speedMin = 7.0f;
    m_updateParams.speedMax = 9.0f;
    m_updateParams.lifeMin = 2.0f;
    m_updateParams.lifeMax = 3.0f;

    m_drawParams.size = 0.03f;
    m_drawParams.intensity = 1.0f;

    // 初期状態を作り、バッファを作るときに一緒に渡す
    std::vector<Particle> initial(m_maxCount);
    MakeInitialState(initial);

    // ----- 粒のバッファ：StructuredBuffer。UAV と SRV の両方を付ける -----
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(Particle) * m_maxCount;
    bd.Usage = D3D11_USAGE_DEFAULT;                    // GPUが読み書きする。CPU経路では UpdateSubresource で上書き
    bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; // 「構造体の配列」として扱う
    bd.StructureByteStride = sizeof(Particle);         // 1要素の大きさ（HLSLの struct と一致させる）

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = initial.data();
    if (FAILED(pDevice->CreateBuffer(&bd, &init, m_particleBuffer.GetAddressOf()))) return false;

    // UAV（CSの u0 用）。構造化バッファの形式は UNKNOWN（形は StructureByteStride で決まる）
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = m_maxCount;
    if (FAILED(pDevice->CreateUnorderedAccessView(m_particleBuffer.Get(), &uavDesc, m_particleUAV.GetAddressOf()))) return false;

    // SRV（VSの t0 用）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = m_maxCount;
    if (FAILED(pDevice->CreateShaderResourceView(m_particleBuffer.Get(), &srvDesc, m_particleSRV.GetAddressOf()))) return false;

    // ----- 定数バッファ -----
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.ByteWidth = sizeof(UpdateCB);
    if (FAILED(pDevice->CreateBuffer(&cbd, nullptr, m_updateCB.GetAddressOf()))) return false;
    cbd.ByteWidth = sizeof(DrawCB);
    if (FAILED(pDevice->CreateBuffer(&cbd, nullptr, m_drawCB.GetAddressOf()))) return false;

    // CPU経路用の配列も同じ初期状態から始める（GPU側へはもう渡し終えたので、中身を移す）
    m_cpuParticles = std::move(initial);
    return true;
}

void ParticleSystem::SetActiveCount(UINT count)
{
    m_activeCount = (std::min)(count, m_maxCount);
}

void ParticleSystem::SetUpdateMode(ParticleUpdateMode mode)
{
    if (mode == m_mode) return;
    m_mode = mode;
    // 切り替えた直後は、GPUのバッファとCPUの配列の中身がずれている
    // （GPU経路で動いている間、CPUの配列は止まったまま）。同じ条件から比べるため、両方を初期状態に戻す
    // ※コンテキストが要るので、実際に戻すのは次の Update の中
    m_resetRequested = true;
}

void ParticleSystem::MakeInitialState(std::vector<Particle>& out) const
{
    // 生まれるまでの待ち時間を [0, 平均寿命) に散らす
    // → 平均寿命のあいだに、毎フレームほぼ同じ数ずつ生まれていく（最初から途切れない噴水になる）
    const float meanLife = (m_updateParams.lifeMin + m_updateParams.lifeMax) * 0.5f;
    for (uint32_t i = 0; i < static_cast<uint32_t>(out.size()); i++) {
        Particle p = Spawn(i, 0u, m_updateParams);               // 種 0 ＝ 初期状態専用
        p.age = -ToUnit(PcgHash(i + 0x9E3779B9u)) * meanLife;   // 負の値＝待ち時間
        out[i] = p;
    }
}

void ParticleSystem::Reset(ID3D11DeviceContext* pContext)
{
    MakeInitialState(m_cpuParticles);
    pContext->UpdateSubresource(m_particleBuffer.Get(), 0, nullptr, m_cpuParticles.data(), 0, 0);
    m_updateParams.frameSeed = 1;
}

void ParticleSystem::Update(ID3D11DeviceContext* pContext)
{
    if (!pContext || !m_particleBuffer) return;

    if (m_resetRequested) {
        Reset(pContext);
        m_resetRequested = false;
    }

    m_updateParams.count = m_activeCount;

    if (m_mode == ParticleUpdateMode::GPU) UpdateGPU(pContext);
    else                                   UpdateCPU(pContext);

    m_updateParams.frameSeed++;
}

void ParticleSystem::UpdateGPU(ID3D11DeviceContext* pContext)
{
    m_lastCpuMs = 0.0f;

    pContext->UpdateSubresource(m_updateCB.Get(), 0, nullptr, &m_updateParams, 0, 0);

    // ※同じバッファを SRV と UAV に同時にはつなげない。VS の t0 は前のフレームの Draw の最後で外してある
    ID3D11Buffer* cb = m_updateCB.Get();
    ID3D11UnorderedAccessView* uav = m_particleUAV.Get();
    pContext->CSSetShader(m_pUpdateCS, nullptr, 0);
    pContext->CSSetConstantBuffers(0, 1, &cb);
    pContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);

    // 1スレッド＝1粒。番号 i → i番目の粒（画像と違って、2次元の対応はない）
    // 切り上げ：割り切れない数でも最後の粒まで届くように。はみ出したスレッドはシェーダー側で止める
    const UINT groups = (m_activeCount + GROUP_SIZE - 1) / GROUP_SIZE;
    if (groups > 0) pContext->Dispatch(groups, 1, 1);

    // 後片付け：この後 VS が SRV として読むので、UAV を外す
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    pContext->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    pContext->CSSetShader(nullptr, nullptr, 0);
}

void ParticleSystem::UpdateCPU(ID3D11DeviceContext* pContext)
{
    // 計るのは「CPUで全粒を進める」＋「GPUへ送る命令を出す（UpdateSubresource の呼び出し）」
    // GPU側で実際にコピーする時間は、Rendererの計測区間（Particles）に出る
    const auto t0 = std::chrono::high_resolution_clock::now();

    for (UINT i = 0; i < m_activeCount; i++) {
        Simulate(m_cpuParticles[i], i, m_updateParams);
    }

    // 動かした範囲（先頭から activeCount 個）だけを送る。単位はバイト
    D3D11_BOX box = {};
    box.left = 0;
    box.right = sizeof(Particle) * m_activeCount;
    box.top = 0;    box.bottom = 1;
    box.front = 0;  box.back = 1;
    if (m_activeCount > 0) {
        pContext->UpdateSubresource(m_particleBuffer.Get(), 0, &box, m_cpuParticles.data(), 0, 0);
    }

    const auto t1 = std::chrono::high_resolution_clock::now();
    m_lastCpuMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void ParticleSystem::Draw(ID3D11DeviceContext* pContext, ID3D11Buffer* perFrameCB)
{
    if (!pContext || m_activeCount == 0) return;

    pContext->UpdateSubresource(m_drawCB.Get(), 0, nullptr, &m_drawParams, 0, 0);

    // 頂点バッファは使わない。点を activeCount 個「描け」とだけ言い、
    // VS が SV_VertexID（0, 1, 2, ...）を番号として粒のバッファから読む
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    m_pDrawShader->Bind(pContext);                 // InputLayout は nullptr（入力なし）、VS、PS
    pContext->GSSetShader(m_pDrawGS, nullptr, 0);

    ID3D11ShaderResourceView* srv = m_particleSRV.Get();
    pContext->VSSetShaderResources(0, 1, &srv);

    // b0（ビュー・射影）は GS が使う。他のパスが付け替えている可能性があるので、ここで付け直す
    pContext->GSSetConstantBuffers(0, 1, &perFrameCB);
    ID3D11Buffer* drawCB = m_drawCB.Get();
    pContext->GSSetConstantBuffers(10, 1, &drawCB);
    pContext->PSSetConstantBuffers(10, 1, &drawCB);

    pContext->Draw(m_activeCount, 0);

    // 後片付け
    //   ・SRV：次のフレームで同じバッファを CS の UAV としてつなぐので外す
    //          （シーンを抜けた後に、消えたバッファが t0 に残り続けないようにする意味もある）
    //   ・GS：次に描く物がGSを通らないように外す
    //   ・トポロジー：他の描画はすべて TriangleList
    ID3D11ShaderResourceView* nullSRV = nullptr;
    pContext->VSSetShaderResources(0, 1, &nullSRV);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
