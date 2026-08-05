#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>
#include"uiLayoutCommon.h"

#include"textRenderer.h"
#include"input.h"
#include"scene1.h"
#include"scene2.h"
#include"scene3.h"
#include"scene4.h"
#include"scene5.h"

class GameObject;

class Scene0 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);
        // 全シーン共通のシーン番号キー判定（IScene::CheckSceneNumberKeys）に一本化
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer,float dt) override {
        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer);
        //現在sceneでの操作説明を表示する
        displayHowToUse(textRenderer,dt);

        //本文
        std::vector<int> posYArray(9);
		int topY = 20;
		int lineSpace = 60;
        for (int i = 0; i < posYArray.size(); i++) {
			posYArray[i] = topY + i * lineSpace;
        }
        textRenderer->DrawString(L"Scene0 - Index", 20.0f, posYArray[0]);
        textRenderer->DrawString(L"Scene1 - Base Rendering", 20.0f, posYArray[1]);
        textRenderer->DrawString(L"Scene2 - Texture and Gamma", 20.0f, posYArray[2]);
        textRenderer->DrawString(L"Scene3 - Deferred Rendering", 20.0f, posYArray[3]);
        textRenderer->DrawString(L"Scene4 - Normal Mapping", 20.0f, posYArray[4]);
        textRenderer->DrawString(L"Scene5 - Shadow Mapping", 20.0f, posYArray[5]);
        textRenderer->DrawString(L"Scene6 - HDR and Bloom", 20.0f, posYArray[6]);
        textRenderer->DrawString(L"Scene7 - GPU Instancing", 20.0f, posYArray[7]);
        textRenderer->DrawString(L"Scene8 - All", 20.0f, posYArray[8]);

    }

    void displayCurrentScene(TextRenderer* textRenderer) {
        int posX = UILayoutCommon::currentScenePositionX;
        int posY = UILayoutCommon::currentScenePositionY;
        textRenderer->DrawString(L"Scene0/8",posX,posY);
    }

    void displayHowToUse(TextRenderer* textRenderer,float dt) {
        int posX = UILayoutCommon::howToUsePositionX;
        int posY = UILayoutCommon::howToUsePositionY;
        int spaceY= UILayoutCommon::howToUseSpaceY;
        float speed = 3.0f;
        float alpha=(sinf(m_uiTime * speed) + 2.0f)*0.5f;
        textRenderer->DrawString(L"Jump to Scene:Press Number Key", posX, posY,
            1.0f,1.0f,1.0f,alpha);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

};