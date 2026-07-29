// scene0.h:それぞれのシーン番号と内容の説明を表示するシーン
#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>
#include"uiLayoutCommon.h"

//test
#include"textRenderer.h"

class GameObject;

class Scene1 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);
        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    // TextRendererの動作確認用の仮実装。Scene0(Title)ができたら削除してよい。
    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene1 - Basic Color", 20.0f, 20.0f);
        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer,1);
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

};