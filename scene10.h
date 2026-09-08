#pragma once
#include "IScene.h"
#include "gameObject.h"
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <DirectXMath.h>
#include "uiLayoutCommon.h"

#include "textRenderer.h"
#include "input.h"
#include "renderer.h"
#include "graphicsCommon.h"

class GameObject;
class Model;

// Scene10 - PBR対照実験デモ
// カメラも光も固定したまま、1つのモデルのmetallic/roughnessだけを
// キー操作でリアルタイムに変える。「同じ光なのに質感だけ変わる」ことを
// その場で見せる／確認するための、Scene9（グリッド）とは補完関係にあるシーン
class Scene10 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // 矢印キーでmetallic/roughnessをリアルタイム変更
        const float paramSpeed = 0.5f;
        if (Input::IsKeyDown(VK_UP))    m_metallic += paramSpeed * dt;
        if (Input::IsKeyDown(VK_DOWN))  m_metallic -= paramSpeed * dt;
        if (Input::IsKeyDown(VK_RIGHT)) m_roughness += paramSpeed * dt;
        if (Input::IsKeyDown(VK_LEFT))  m_roughness -= paramSpeed * dt;

        if (m_metallic > 1.0f) m_metallic = 1.0f;
        if (m_metallic < 0.0f) m_metallic = 0.0f;
        if (m_roughness > 1.0f) m_roughness = 1.0f;
        if (m_roughness < 0.02f) m_roughness = 0.02f; // 0だとハイライトが不安定になるため下限を設ける

        // 毎フレーム、今のパラメータをモデルへ反映
        if (m_targetModel) {
            m_targetModel->SetMetallicRoughness(m_metallic, m_roughness);
        }

        // WASDでディレクショナルライトの向きも動かせる（Scene9と同じ操作感）
        const float dirSpeed = 1.0f;
        if (Input::IsKeyDown('A')) m_dirX += dirSpeed * dt;
        if (Input::IsKeyDown('D')) m_dirX -= dirSpeed * dt;
        if (Input::IsKeyDown('W')) m_dirY -= dirSpeed * dt;
        if (Input::IsKeyDown('S')) m_dirY += dirSpeed * dt;
        if (m_dirX > 1.0f) m_dirX = 1.0f;
        if (m_dirX < -1.0f) m_dirX = -1.0f;
        if (m_dirY > 1.0f) m_dirY = 1.0f;
        if (m_dirY < -1.0f) m_dirY = -1.0f;

        // Tabキーでデバッグビュー切り替え（Scene9と同じ並び）
        if (Input::IsKeyPressed(VK_TAB)) {
            switch (m_debugView) {
            case GBufferDebugView::Lit:       m_debugView = GBufferDebugView::Metallic;  break;
            case GBufferDebugView::Metallic:  m_debugView = GBufferDebugView::Roughness; break;
            case GBufferDebugView::Roughness: m_debugView = GBufferDebugView::Albedo;    break;
            case GBufferDebugView::Albedo:    m_debugView = GBufferDebugView::Normal;    break;
            case GBufferDebugView::Normal:    m_debugView = GBufferDebugView::Depth;     break;
            case GBufferDebugView::Depth:     m_debugView = GBufferDebugView::Lit;       break;
            }
        }

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
        renderer->SetDebugView(m_debugView);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene10 - PBR Interactive (Controlled Comparison)", 20.0f, 20.0f);
        textRenderer->DrawString(L"Up/Down : Metallic   Left/Right : Roughness", 20.0f, 80.0f);
        textRenderer->DrawString(L"WASD : Move Directional Light   Tab : Switch View", 20.0f, 140.0f);

        std::wstringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << L"Metallic = " << m_metallic << L"   Roughness = " << m_roughness;
        textRenderer->DrawString(ss.str().c_str(), 20.0f, 200.0f);

        m_uiTime += dt;
        displayCurrentScene(textRenderer, 10);
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    Model* m_targetModel = nullptr; // ★所有権はm_objects側（GameObjectが借用）。値変更のためだけに参照を持つ

    float m_metallic = 0.0f;
    float m_roughness = 0.5f;

    float m_dirX = 0.3f;
    float m_dirY = -0.6f;

    GBufferDebugView m_debugView = GBufferDebugView::Lit;
};
