// scene2.h：Scene2 - テクスチャ＋ガンマ補正のON/OFF切り替え
#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>
#include"uiLayoutCommon.h"

//test
#include"textRenderer.h"
#include"input.h"
#include"renderer.h" // SetGammaCorrection

class GameObject;

class Scene2 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // Tabキーでガンマ補正のON/OFFを切り替える
        if (Input::IsKeyPressed(VK_TAB)) {
            m_gammaOn = !m_gammaOn;
        }

        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        // ガンマ補正の状態はRenderer（ScreenBlitパス）に伝える
        renderer->SetGammaCorrection(m_gammaOn);
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene2 - Texture and Gamma", 20.0f, 20.0f);
        textRenderer->DrawString(
            m_gammaOn ? L"Gamma Correction : ON" : L"Gamma Correction : OFF",
            20.0f, 80.0f);
        textRenderer->DrawString(L"Tab : Toggle Gamma Correction", 20.0f, 140.0f);

        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer,2);
        //現在sceneでの操作説明を表示する
        displayHowToUse(textRenderer, dt);
        //test
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    //固有
    bool m_gammaOn = true;
};
