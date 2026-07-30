// scene7.h：Scene7 - GPUインスタンシング（P/Mでインスタンス数を増減、上限512）
#pragma once
#include "IScene.h"
#include "uiLayoutCommon.h"

#include "textRenderer.h"
#include "input.h"
#include "renderer.h"

class Scene7 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        // ===== インスタンス数の増減（P=+ / M=-。Scene6のExposureと同じ「押しっぱなしで連続変化」方式）=====
        const float countSpeed = 60.0f; // 1秒あたりの増減数
        if (Input::IsKeyDown('P')) m_instanceCountF += countSpeed * dt;
        if (Input::IsKeyDown('M')) m_instanceCountF -= countSpeed * dt;
        if (m_instanceCountF > static_cast<float>(kMaxInstances)) m_instanceCountF = static_cast<float>(kMaxInstances);
        if (m_instanceCountF < 0.0f) m_instanceCountF = 0.0f;

        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        renderer->SetInstanceCount(static_cast<UINT>(m_instanceCountF));
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene7 - GPU Instancing", 20.0f, 20.0f);
        textRenderer->DrawString(L"P / M : Increase / Decrease Instance Count", 20.0f, 80.0f);

        wchar_t buf[64];
        swprintf_s(buf, L"Instance Count : %d / %d", static_cast<int>(m_instanceCountF), kMaxInstances);
        textRenderer->DrawString(buf, 20.0f, 140.0f);

        m_uiTime += dt;
        // 現在のscene番号表示
        displayCurrentScene(textRenderer, 7);
        // 数字キーでの遷移方法表示
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    static constexpr int kMaxInstances = 512;
    // 初期値は旧テストコードと同じ3^3=27個から開始
    float m_instanceCountF = 27.0f;
};