// scene8.h：Scene8 - 統合デモ（「同時に動くこと自体が証明になる」技術のみで構成。
// ON/OFF切替そのものが本質だった機能[テクスチャ+ガンマ切替/G-Bufferデバッグ表示]は対象外）
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

class Scene8 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // ===== DirectionalLight操作（Scene4/5と同じロジック）=====
        const float dirSpeed = 1.0f;
        if (Input::IsKeyDown('A')) m_dirX += dirSpeed * dt;
        if (Input::IsKeyDown('D')) m_dirX -= dirSpeed * dt;
        if (Input::IsKeyDown('W')) m_dirY -= dirSpeed * dt;
        if (Input::IsKeyDown('S')) m_dirY += dirSpeed * dt;
        if (m_dirX > 1.0f) m_dirX = 1.0f;
        if (m_dirX < -1.0f) m_dirX = -1.0f;
        if (m_dirY > 1.0f) m_dirY = 1.0f;
        if (m_dirY < -1.0f) m_dirY = -1.0f;

        // ===== PointLight操作（Scene5/6と同じロジック）=====
        // このライトはシャドウ判定と同時にHDR+Bloomの発火トリガーも兼ねる
        const float pointSpeed = 4.0f;
        if (Input::IsKeyDown('J')) m_pointX -= pointSpeed * dt;
        if (Input::IsKeyDown('L')) m_pointX += pointSpeed * dt;
        if (Input::IsKeyDown('I')) m_pointZ += pointSpeed * dt;
        if (Input::IsKeyDown('K')) m_pointZ -= pointSpeed * dt;
        if (m_pointX > 6.0f) m_pointX = 6.0f;
        if (m_pointX < -6.0f) m_pointX = -6.0f;
        if (m_pointZ > 6.0f) m_pointZ = 6.0f;
        if (m_pointZ < -6.0f) m_pointZ = -6.0f;

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

        DirectX::XMFLOAT3 pointPos(m_pointX, m_pointHeight, m_pointZ);
        renderer->SetPointLightPosition(pointPos);

        // 統合デモでは両ライト同時表示が前提のため、切替なしで固定
        renderer->SetLightVisibilityMode(LightVisibilityMode::Both);

        // Exposure / Bloomは固定（統合デモの主眼ではないため操作不可）
        renderer->SetExposure(m_exposure);
        renderer->SetBloomActive(true);

        // GPUインスタンシング：奥へオフセットした背景の群として固定個数を表示
        renderer->SetInstanceCount(m_instanceCount, m_instanceGroupOffset);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene8 - Integrated Demo", 20.0f, 20.0f);
        textRenderer->DrawString(L"WASD : Move Directional Light (X/Y)", 20.0f, 80.0f);
        textRenderer->DrawString(L"IJKL : Move Point Light (X/Z)", 20.0f, 140.0f);

        wchar_t buf[128];
        swprintf_s(buf, L"Dir Light : (%.2f, %.2f)", m_dirX, m_dirY);
        textRenderer->DrawString(buf, 20.0f, 200.0f);

        swprintf_s(buf, L"Point Light : (%.2f, %.2f)", m_pointX, m_pointZ);
        textRenderer->DrawString(buf, 20.0f, 260.0f);

        m_uiTime += dt;
        displayCurrentScene(textRenderer, 8);
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    float m_dirX = 3.0f;
    float m_dirY = -1.0f;

    float m_pointX = 3.0f;
    float m_pointZ = 3.0f;
    const float m_pointHeight = 4.0f;

    // Scene6の初期値に合わせて固定
    const float m_exposure = 0.5f;

    // 表示数・奥へのオフセットは固定（背景の群として使うため操作不可）
    static constexpr UINT m_instanceCount = 125; // 5x5x5
    const DirectX::XMFLOAT3 m_instanceGroupOffset = DirectX::XMFLOAT3(0.0f, 0.0f, -40.0f);
};