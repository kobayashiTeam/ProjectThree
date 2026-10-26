#pragma once
#include "IScene.h"
#include "gameObject.h"
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <DirectXMath.h>
#include "uiLayoutCommon.h"

#include "textRenderer.h"
#include "input.h"
#include "renderer.h"
#include "graphicsCommon.h"
#include "particleSystem.h"

class GameObject;

// Scene12 - GPUパーティクル（コンピュートシェーダー）
// 床の上に噴水を置き、粒の更新を GPU（CS）と CPU（C++のループ＋毎フレーム転送）で切り替えて比べる
//   ・G：更新の方式 GPU ⇔ CPU（切り替えると両方とも初期状態からやり直す）
//   ・N：粒の数 1万 → 10万 → 100万
// 計測：F4 で VSync を切り、GPU の表の Particles（更新）と Forward（描画を含む）、画面の CPU ms を見る
//   ※CPU の ms は Debug ビルドだと最適化が効かず大きく出るので、比べるときは Release で
class Scene12 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // G：GPU ⇔ CPU
        if (Input::IsKeyPressed('G')) {
            m_mode = (m_mode == ParticleUpdateMode::GPU) ? ParticleUpdateMode::CPU : ParticleUpdateMode::GPU;
        }
        // N：粒の数
        if (Input::IsKeyPressed('N')) {
            m_countIndex = (m_countIndex + 1) % COUNT_LEVELS;
        }

        if (m_particles) {
            m_particles->SetDeltaTime(dt);
            m_particles->SetUpdateMode(m_mode);
            m_particles->SetActiveCount(COUNTS[m_countIndex]);
            // 数が多いほど重なりが増えて明るくなりすぎるので、小さく・暗くする
            m_particles->SetDrawParams(SIZES[m_countIndex], INTENSITIES[m_countIndex]);
        }

        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        DirectX::XMFLOAT3 dir(0.4f, -0.8f, 0.5f);
        DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir));
        DirectX::XMStoreFloat3(&dir, v);
        renderer->SetDirectionalLightDirection(dir);
        renderer->SetLightVisibilityMode(LightVisibilityMode::DirectionalOnly);
        renderer->SetDebugView(GBufferDebugView::Lit);

        // パーティクルはBeginFrameで毎フレーム外れるので、ここで毎フレーム渡す
        renderer->SubmitParticles(m_particles.get());

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene12 - GPU Particles (Compute Shader)", 20.0f, 20.0f);
        textRenderer->DrawString(L"G : Update GPU / CPU   N : Particle Count", 20.0f, 80.0f);

        std::wstringstream ss;
        ss << L"Update : " << (m_mode == ParticleUpdateMode::GPU ? L"GPU (CS)" : L"CPU (loop + UpdateSubresource)");
        ss << L"   Count : " << COUNTS[m_countIndex];
        textRenderer->DrawString(ss.str().c_str(), 20.0f, 140.0f);

        if (m_mode == ParticleUpdateMode::CPU && m_particles) {
            // 1フレームごとの値はちらつくので、30フレームの平均を表示する
            m_cpuMsAccum += m_particles->GetLastCpuUpdateMs();
            if (++m_cpuMsFrames >= 30) {
                m_cpuMsDisplay = m_cpuMsAccum / m_cpuMsFrames;
                m_cpuMsAccum = 0.0f;
                m_cpuMsFrames = 0;
            }
            std::wstringstream ss2;
            ss2 << std::fixed << std::setprecision(2) << L"CPU update : " << m_cpuMsDisplay << L" ms";
            textRenderer->DrawString(ss2.str().c_str(), 20.0f, 200.0f);
        }
        else {
            m_cpuMsAccum = 0.0f;
            m_cpuMsFrames = 0;
            m_cpuMsDisplay = 0.0f;
        }

        m_uiTime += dt;
        displayCurrentScene(textRenderer, 12);
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    // 粒の数の段階（バッファは最大数で1回だけ作り、動かす数だけを変える）
    static constexpr int COUNT_LEVELS = 3;
    static constexpr UINT  COUNTS[COUNT_LEVELS] = { 10000, 100000, 1000000 };
    static constexpr float SIZES[COUNT_LEVELS] = { 0.05f, 0.035f, 0.02f };
    static constexpr float INTENSITIES[COUNT_LEVELS] = { 1.0f, 0.35f, 0.12f };

    // このシーンが持つ（最大100万個で32MB。他のシーンでは要らないので、シーンと一緒に消える）
    std::unique_ptr<ParticleSystem> m_particles;
    ParticleUpdateMode m_mode = ParticleUpdateMode::GPU;
    int m_countIndex = 1; // 10万から始める

    // CPU ms の平均表示
    float m_cpuMsAccum = 0.0f;
    int   m_cpuMsFrames = 0;
    float m_cpuMsDisplay = 0.0f;
};
