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

class Scene6 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // ===== PointLight操作（IJKLでposition.x/zをワールド空間で絶対移動。Scene5と同じロジック）=====
        // 中心（0,0）に近づけるほど減衰(attenuation)が小さくなり、輝度が1.0を超えてBloomが発火しやすくなる
        const float pointSpeed = 4.0f;
        if (Input::IsKeyDown('J')) m_pointX -= pointSpeed * dt;
        if (Input::IsKeyDown('L')) m_pointX += pointSpeed * dt;
        if (Input::IsKeyDown('I')) m_pointZ += pointSpeed * dt;
        if (Input::IsKeyDown('K')) m_pointZ -= pointSpeed * dt;
        if (m_pointX > 4.0f) m_pointX = 4.0f;
        if (m_pointX < -4.0f) m_pointX = -4.0f;
        if (m_pointZ > 4.0f) m_pointZ = 4.0f;
        if (m_pointZ < -4.0f) m_pointZ = -4.0f;

        // ===== 露出（Exposure）調整：P=+、M=- =====
        const float exposureSpeed = 0.6f;
        if (Input::IsKeyDown('P')) m_exposure += exposureSpeed * dt;
        if (Input::IsKeyDown('M')) m_exposure -= exposureSpeed * dt;
        if (m_exposure > 3.0f) m_exposure = 3.0f;
        if (m_exposure < 0.1f) m_exposure = 0.1f;

        // ===== Bloom ON/OFF切り替え（Tab） =====
        if (Input::IsKeyPressed(VK_TAB)) {
            m_bloomActive = !m_bloomActive;
        }

        // 全シーン共通のシーン番号キー判定
        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        // PointLight：高さ固定でX/Zのみ操作
        DirectX::XMFLOAT3 pointPos(m_pointX, m_pointHeight, m_pointZ);
        renderer->SetPointLightPosition(pointPos);

        renderer->SetExposure(m_exposure);
        renderer->SetBloomActive(m_bloomActive);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene6 - HDR and Bloom", 20.0f, 20.0f);
        textRenderer->DrawString(L"IJKL : Move Point Light closer / away (X/Z)", 20.0f, 80.0f);
        textRenderer->DrawString(L"P / M : Exposure Up / Down", 20.0f, 140.0f);
        textRenderer->DrawString(L"Tab  : Bloom ON / OFF", 20.0f, 200.0f);

        wchar_t buf[128];
        swprintf_s(buf, L"Point Light : (%.2f, %.2f)", m_pointX, m_pointZ);
        textRenderer->DrawString(buf, 20.0f, 260.0f);

        swprintf_s(buf, L"Exposure : %.2f", m_exposure);
        textRenderer->DrawString(buf, 20.0f, 320.0f);

        swprintf_s(buf, L"Bloom : %ls", m_bloomActive ? L"ON" : L"OFF");
        textRenderer->DrawString(buf, 20.0f, 380.0f);

        // トータル時間の更新
        m_uiTime += dt;
        // 現在のsceneを表示する
        displayCurrentScene(textRenderer, 6);
        // 現在sceneでの操作説明を表示する
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    // PointLight：position.x / position.z（高さは固定・浮遊cubeのすぐ上に設定）
    float m_pointX = 3.0f;
    float m_pointZ = 3.0f;
    const float m_pointHeight = 2.0f;

    // 露出（Exposure）：Rendererの初期値(0.5f)に合わせる
    float m_exposure = 0.5f;

    // Bloom ON/OFF（BloomCombinePostProcessの初期値(true)に合わせる）
    bool m_bloomActive = true;
};
