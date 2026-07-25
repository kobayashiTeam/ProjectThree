// TestScene.h
#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>

//test
#include"textRenderer.h"

class GameObject;

class TestScene : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);
    }

    void Submit(Renderer* renderer) override {
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    // TextRendererの動作確認用の仮実装。Scene0(Title)ができたら削除してよい。
    void SubmitUI(TextRenderer* textRenderer,float dt) override {
        textRenderer->DrawString(L"TestScene - TextRenderer OK", 20.0f, 20.0f);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;
};