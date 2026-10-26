// =========================================================
// GPUパーティクルの更新（噴水）
//
// 1スレッド ＝ 1粒。番号 i のスレッドが、i 番目の粒を読んで、動かして、同じ場所に書き戻す
//   ・他の粒を読まないので、1本のバッファに書き戻してよい（実行の順番に結果が左右されない）
//   ・groupshared もバリアも使わないので、はみ出したスレッドは最初に return してよい
//
// CPU経路（particleSystem.cpp の Simulate）と同じ手順。片方を変えたら、もう片方も変える
// =========================================================

// 1グループのスレッド数。ParticleSystem::GROUP_SIZE と必ずそろえる
#define GROUP_SIZE 256

// ParticleSystem::Particle と同じ並び（ParticleDraw.hlsl にも同じものがある）
struct Particle
{
    float3 position;
    float age; // 負の値＝まだ生まれていない（待ち時間）
    float3 velocity;
    float lifetime;
};

// RW ＝ 読み書きできる。C++ 側では、このバッファの UAV を u0 につなぐ
RWStructuredBuffer<Particle> g_particles : register(u0);

// ParticleSystem::UpdateCB と同じ並び
cbuffer ParticleUpdateCB : register(b0)
{
    float3 g_emitterPos;
    float g_dt;
    float3 g_gravity;
    uint g_count;
    uint g_frameSeed;
    float g_floorY;
    float g_bounce;
    float g_spread;
    float g_speedMin;
    float g_speedMax;
    float g_lifeMin;
    float g_lifeMax;
};

// PCGハッシュ：番号を混ぜて、ばらばらに見える32ビットの値にする
uint PcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// 上位24ビットを使って [0, 1) の小数にする
float ToUnit(uint h)
{
    return (float) (h >> 8) * (1.0f / 16777216.0f);
}

// 粒 i を発生点に生まれ変わらせる（age = 0）
Particle Spawn(uint i, uint seed)
{
    uint h = PcgHash(i ^ PcgHash(seed));
    float r0 = ToUnit(h);
    h = PcgHash(h);
    float r1 = ToUnit(h);
    h = PcgHash(h);
    float r2 = ToUnit(h);
    h = PcgHash(h);
    float r3 = ToUnit(h);

    // 横の初速は円の中に一様に散らす（半径に sqrt を掛けないと中心に偏る）
    float angle = r0 * 6.2831853f;
    float radius = g_spread * sqrt(r1);

    Particle p;
    p.position = g_emitterPos;
    p.velocity = float3(cos(angle) * radius, lerp(g_speedMin, g_speedMax, r2), sin(angle) * radius);
    p.age = 0.0f;
    p.lifetime = lerp(g_lifeMin, g_lifeMax, r3);
    return p;
}

// 1粒を1フレームぶん進める
Particle Simulate(Particle p, uint i)
{
    p.age += g_dt;
    if (p.age < 0.0f)
        return p; // まだ生まれていない：発生点で待つ
    if (p.age >= p.lifetime)
        return Spawn(i, g_frameSeed); // 寿命が尽きた：その場で生まれ変わる

    p.velocity += g_gravity * g_dt;
    p.position += p.velocity * g_dt;

    // 床で跳ね返る（縦は g_bounce の割合だけ残して反転、横は摩擦で 0.8 倍。0.8 はC++とそろえる）
    if (p.position.y < g_floorY && p.velocity.y < 0.0f)
    {
        p.position.y = g_floorY;
        p.velocity.y *= -g_bounce;
        p.velocity.xz *= 0.8f;
    }
    return p;
}

// 1列（256人）のグループ。画像のときと違い、番号に「横・縦」の意味はない
// SV_DispatchThreadID.x がそのまま粒の番号になる
[numthreads(GROUP_SIZE, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint i = dtid.x;
    // グループ数を切り上げたぶん、はみ出したスレッドは何もしない
    // （共有の配列もバリアもないので、誰もこのスレッドを待っていない）
    if (i >= g_count)
        return;

    g_particles[i] = Simulate(g_particles[i], i);
}
