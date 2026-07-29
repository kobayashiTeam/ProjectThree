// scene3.h：Scene3 - 遅延レンダリング（G-Bufferのアルベド/法線/深度切り替え表示）
#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>
#include"uiLayoutCommon.h"

//test
#include"textRenderer.h"
#include"input.h"
#include"renderer.h" // GBufferDebugView

class GameObject;

class Scene3 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // Tabキーでモードを Lit → Albedo → Normal → Depth → Lit … と切り替える
        if (Input::IsKeyPressed(VK_TAB)) {
            int next = (static_cast<int>(m_debugView) + 1) % 4;
            m_debugView = static_cast<GBufferDebugView>(next);
        }

        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        // 案A：表示モードはRenderer側の状態として伝える
        renderer->SetDebugView(m_debugView);
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene3 - Deferred Rendering", 20.0f, 20.0f);
        textRenderer->DrawString(GetModeLabel(), 20.0f, 80.0f);
        textRenderer->DrawString(L"Tab : Switch View (Lit / Albedo / Normal / Depth)", 20.0f, 140.0f);
    
        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer, 3);
        //現在sceneでの操作説明を表示する
        displayHowToUse(textRenderer, dt);
        //test
        DrawSceneNavigationHint(textRenderer);
    }

private:
    const wchar_t* GetModeLabel() const {
        switch (m_debugView) {
        case GBufferDebugView::Lit:    return L"View : Lit (Final)";
        case GBufferDebugView::Albedo: return L"View : Albedo";
        case GBufferDebugView::Normal: return L"View : Normal";
        case GBufferDebugView::Depth:  return L"View : Depth";
        default: return L"";
        }
    }

    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    //固有
    GBufferDebugView m_debugView = GBufferDebugView::Lit;
};
