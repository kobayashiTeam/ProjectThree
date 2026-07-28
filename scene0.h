// scene0.h:それぞれのシーン番号と内容の説明を表示するシーン
#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>
#include"uiLayoutCommon.h"

//test
#include"textRenderer.h"
#include"input.h"
#include"scene1.h"
#include"scene2.h"
#include"scene3.h"

class GameObject;

class Scene0 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);
        //test:input加入
        if (Input::IsKeyPressed('1')) {
            m_nextScene = new Scene1();
        }
        if (Input::IsKeyPressed('2')) {
            m_nextScene = new Scene2();
        }
        //Scene2（テクスチャ＋ガンマ）はまだ未実装のため、先にScene3へ
        if (Input::IsKeyPressed('3')) {
            m_nextScene = new Scene3();
        }
    }

    void Submit(Renderer* renderer) override {
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    // TextRendererの動作確認用の仮実装。Scene0(Title)ができたら削除してよい。
    void SubmitUI(TextRenderer* textRenderer,float dt) override {
        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer);
        //現在sceneでの操作説明を表示する
        displayHowToUse(textRenderer,dt);

        //本分
        std::vector<int> posYArray(9);
		int topY = 20;
		int lineSpace = 60;
        for (int i = 0; i < posYArray.size(); i++) {
			posYArray[i] = topY + i * lineSpace;
        }
        textRenderer->DrawString(L"Scene0 - Index", 20.0f, posYArray[0]);//20
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
        textRenderer->DrawString(L"Scene0/8",posX,posY);//1000,20
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

    //IScene* CheckTransition() override{ return nullptr; }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    //固有
    float m_uiTime=0.0f;
};