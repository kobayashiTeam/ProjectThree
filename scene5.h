// scene5.h：Scene5 - シャドウマッピング（DirectionalLightをWASD、PointLightをIJKLで動かし、Tabで表示ライトを切り替える）
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

class Scene5 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // ===== DirectionalLight操作（Scene4と同じロジック：WASDでdirection.x/yを増減）=====
        const float dirSpeed = 1.0f;
        if (Input::IsKeyDown('A')) m_dirX += dirSpeed * dt;
        if (Input::IsKeyDown('D')) m_dirX -= dirSpeed * dt;
        if (Input::IsKeyDown('W')) m_dirY -= dirSpeed * dt;
        if (Input::IsKeyDown('S')) m_dirY += dirSpeed * dt;
        if (m_dirX > 1.0f) m_dirX = 1.0f;
        if (m_dirX < -1.0f) m_dirX = -1.0f;
        if (m_dirY > 1.0f) m_dirY = 1.0f;
        if (m_dirY < -1.0f) m_dirY = -1.0f;

        // ===== PointLight操作（IJKLでposition.x/zをワールド空間で絶対移動）=====
        const float pointSpeed = 4.0f;
        if (Input::IsKeyDown('J')) m_pointX -= pointSpeed * dt;
        if (Input::IsKeyDown('L')) m_pointX += pointSpeed * dt;
        if (Input::IsKeyDown('I')) m_pointZ += pointSpeed * dt;
        if (Input::IsKeyDown('K')) m_pointZ -= pointSpeed * dt;
        if (m_pointX > 6.0f) m_pointX = 6.0f;
        if (m_pointX < -6.0f) m_pointX = -6.0f;
        if (m_pointZ > 6.0f) m_pointZ = 6.0f;
        if (m_pointZ < -6.0f) m_pointZ = -6.0f;

        // ===== 表示ライト切り替え（Tab：Directional Only → Point Only → Both →循環）=====
        if (Input::IsKeyPressed(VK_TAB)) {
            switch (m_lightVisibilityMode) {
            case LightVisibilityMode::DirectionalOnly:
                m_lightVisibilityMode = LightVisibilityMode::PointOnly;
                break;
            case LightVisibilityMode::PointOnly:
                m_lightVisibilityMode = LightVisibilityMode::Both;
                break;
            case LightVisibilityMode::Both:
                m_lightVisibilityMode = LightVisibilityMode::DirectionalOnly;
                break;
            }
        }

        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        // DirectionalLight：z0固定で正規化してから反映（0ベクトル化を回避）
        DirectX::XMFLOAT3 dir(m_dirX, m_dirY, 1.0f);
        float lenSq = dir.x * dir.x + dir.y * dir.y;
        if (lenSq > 0.0001f) {
            DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir));
            DirectX::XMStoreFloat3(&dir, v);
            renderer->SetDirectionalLightDirection(dir);
        }

        // PointLight：高さ固定でX/Zのみ操作
        DirectX::XMFLOAT3 pointPos(m_pointX, m_pointHeight, m_pointZ);
        renderer->SetPointLightPosition(pointPos);

        renderer->SetLightVisibilityMode(m_lightVisibilityMode);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene5 - Shadow Mapping", 20.0f, 20.0f);
        textRenderer->DrawString(L"WASD : Move Directional Light (X/Y)", 20.0f, 80.0f);
        textRenderer->DrawString(L"IJKL : Move Point Light (X/Z)", 20.0f, 140.0f);
        textRenderer->DrawString(L"Tab  : Switch Light (Directional / Point / Both)", 20.0f, 200.0f);

        const wchar_t* modeStr = L"Both";
        if (m_lightVisibilityMode == LightVisibilityMode::DirectionalOnly) modeStr = L"Directional Only";
        else if (m_lightVisibilityMode == LightVisibilityMode::PointOnly) modeStr = L"Point Only";

        wchar_t buf[128];
        swprintf_s(buf, L"Mode : %ls", modeStr);
        textRenderer->DrawString(buf, 20.0f, 260.0f);

        swprintf_s(buf, L"Dir Light : (%.2f, %.2f)", m_dirX, m_dirY);
        textRenderer->DrawString(buf, 20.0f, 320.0f);

        swprintf_s(buf, L"Point Light : (%.2f, %.2f)", m_pointX, m_pointZ);
        textRenderer->DrawString(buf, 20.0f, 380.0f);

        //トータル時間の更新
        m_uiTime += dt;
        //現在のsceneを表示する
        displayCurrentScene(textRenderer, 5);
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

    // DirectionalLight：direction.x / direction.y（正規化前の生値）
    // 初期値はShadowSystem::Initialize()内のDirectionalLightデフォルト方向(3,-1,1)と一致させる
    float m_dirX = 3.0f;
    float m_dirY = -1.0f;

    // PointLight：position.x / position.z（高さは固定）
    float m_pointX = 3.0f;
    float m_pointZ = 3.0f;
    const float m_pointHeight = 4.0f;

    LightVisibilityMode m_lightVisibilityMode = LightVisibilityMode::Both;
};
