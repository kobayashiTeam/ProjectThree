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
#include "graphicsCommon.h"

class GameObject;

// Scene9 - PBRマテリアルボール
// metallic（横方向）とroughness（縦方向）を振ったグリッドを並べて、
// Cook-Torrance BRDFの見た目の変化を一覧できるデモシーン
class Scene9 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // Directional Lightの向きを操作（ハイライトを動かしてroughnessの違いを見る用）
        const float dirSpeed = 1.0f;
        if (Input::IsKeyDown('A')) m_dirX += dirSpeed * dt;
        if (Input::IsKeyDown('D')) m_dirX -= dirSpeed * dt;
        if (Input::IsKeyDown('W')) m_dirY -= dirSpeed * dt;
        if (Input::IsKeyDown('S')) m_dirY += dirSpeed * dt;
        if (m_dirX > 1.0f) m_dirX = 1.0f;
        if (m_dirX < -1.0f) m_dirX = -1.0f;
        if (m_dirY > 1.0f) m_dirY = 1.0f;
        if (m_dirY < -1.0f) m_dirY = -1.0f;

        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        DirectX::XMFLOAT3 dir(m_dirX, m_dirY, 1.0f);
        float lenSq = dir.x * dir.x + dir.y * dir.y;
        if (lenSq > 0.0001f) {
            DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir));
            DirectX::XMStoreFloat3(&dir, v);
            renderer->SetDirectionalLightDirection(dir);
        }
        renderer->SetLightVisibilityMode(LightVisibilityMode::DirectionalOnly);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene9 - PBR Material Grid", 20.0f, 20.0f);
        textRenderer->DrawString(L"WASD : Move Directional Light", 20.0f, 80.0f);
        textRenderer->DrawString(L"-> Metallic increases left to right (0.0 - 1.0)", 20.0f, 140.0f);
        textRenderer->DrawString(L"v  Roughness increases top to bottom (0.1 - 0.9)", 20.0f, 200.0f);

        m_uiTime += dt;
        displayCurrentScene(textRenderer, 9);
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    float m_dirX = 0.3f;
    float m_dirY = -0.6f;
};
