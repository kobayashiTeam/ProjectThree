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

        //  追加：Tabキーで Lit → Metallic → Roughness → Albedo → Normal → Depth → Lit … と切り替える
        // Metallic/Roughnessを先頭に持ってきているのは、Scene9で一番確認したいのがこの2つだから
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

        //  追加：デバッグ表示モードをRendererに伝える
        renderer->SetDebugView(m_debugView);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene9 - PBR Material Grid", 20.0f, 20.0f);
        textRenderer->DrawString(L"WASD : Move Directional Light", 20.0f, 80.0f);
        textRenderer->DrawString(L"Tab : Switch View (Lit / Metallic / Roughness / Albedo / Normal / Depth)", 20.0f, 140.0f);
        textRenderer->DrawString(GetModeLabel(), 20.0f, 200.0f);
        textRenderer->DrawString(L"-> Metallic increases left to right (0.0 - 1.0)", 20.0f, 260.0f);
        textRenderer->DrawString(L"v  Roughness increases top to bottom (0.1 - 0.9)", 20.0f, 320.0f);

        m_uiTime += dt;
        displayCurrentScene(textRenderer, 9);
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    const wchar_t* GetModeLabel() const {
        switch (m_debugView) {
        case GBufferDebugView::Lit:       return L"View : Lit (Final)";
        case GBufferDebugView::Metallic:  return L"View : Metallic (raw G-Buffer alpha)";
        case GBufferDebugView::Roughness: return L"View : Roughness (raw G-Buffer alpha)";
        case GBufferDebugView::Albedo:    return L"View : Albedo";
        case GBufferDebugView::Normal:    return L"View : Normal";
        case GBufferDebugView::Depth:     return L"View : Depth";
        default: return L"";
        }
    }

    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    float m_dirX = 0.3f;
    float m_dirY = -0.6f;

    //  追加：このシーン専用のデバッグ表示状態
    GBufferDebugView m_debugView = GBufferDebugView::Lit;
};
