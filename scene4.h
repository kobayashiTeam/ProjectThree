// scene4.h：Scene4 - 法線マッピング（DirectionalLightをWASDで動かして陰影変化を確認）
#pragma once
#include "IScene.h"
#include "gameObject.h"
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include "uiLayoutCommon.h"

#include "textRenderer.h"
#include "input.h"
#include "renderer.h"

class GameObject;

class Scene4 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        const float speed = 1.0f; // 1秒あたりの変化量
        if (Input::IsKeyDown('A')) m_dirX += speed * dt;
        if (Input::IsKeyDown('D')) m_dirX -= speed * dt;
        if (Input::IsKeyDown('W')) m_dirY -= speed * dt;
        if (Input::IsKeyDown('S')) m_dirY += speed * dt;

        // ±1.0にクランプ
        if (m_dirX > 1.0f) m_dirX = 1.0f;
        if (m_dirX < -1.0f) m_dirX = -1.0f;
        if (m_dirY > 1.0f) m_dirY = 1.0f;
        if (m_dirY < -1.0f) m_dirY = -1.0f;

        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        // ゼロベクトル回避：両軸が0付近の間は直前有効値を維持し、書き換えをスキップ
        DirectX::XMFLOAT3 dir(m_dirX, m_dirY, 1.0f);//z0
        float lenSq = dir.x * dir.x + dir.y * dir.y;
        if (lenSq > 0.0001f) {
            DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir));
            DirectX::XMStoreFloat3(&dir, v);
            renderer->SetDirectionalLightDirection(dir);
        }

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene4 - Normal Mapping", 20.0f, 20.0f);
        textRenderer->DrawString(L"WASD : Move Directional Light (X/Y)", 20.0f, 80.0f);

        wchar_t buf[64];
        swprintf_s(buf, L"Light Dir : (%.2f, %.2f)", m_dirX, m_dirY);
        textRenderer->DrawString(buf, 20.0f, 140.0f);

        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer, 4);
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

    // 固有：DirectionalLightのdirection.x / direction.y（正規化前の生値）
    float m_dirX = 3.0f;//0から3へ
    float m_dirY = -1.0f; // 初期値は既存デフォルト方向(0,-1,0)に一致
};