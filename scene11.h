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

// Scene11 - カスケードシャドウマップ（CSM）
// 奥行き60m程度の長い床と柱の列を置き、
//   ・C  ：CSMと旧シャドウマップ（20x20固定の1枚）を切り替えて、遠くの影が消える／残るを見比べる
//   ・Tab：段ごとに赤・緑・青で色を付け、視錐台がどこで切り分けられているかを見せる
//   ・N/M：分割の混ぜ具合λを変え、境界がリアルタイムに動く様子を見せる
// 矢印キーでカメラを回すと、球＋テクセルスナップにより影の縁がちらつかないことも確認できる
class Scene11 : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);

        // C：CSM ON/OFF（OFF時は従来の20x20シャドウマップ）
        if (Input::IsKeyPressed('C')) m_cascadeOn = !m_cascadeOn;
        // Tab：段ごとの色分け表示
        if (Input::IsKeyPressed(VK_TAB)) m_cascadeDebug = !m_cascadeDebug;

        // N/M：λ（0=等間隔、1=対数）
        const float lambdaSpeed = 0.5f;
        if (Input::IsKeyDown('M')) m_lambda += lambdaSpeed * dt;
        if (Input::IsKeyDown('N')) m_lambda -= lambdaSpeed * dt;
        if (m_lambda > 1.0f) m_lambda = 1.0f;
        if (m_lambda < 0.0f) m_lambda = 0.0f;

        // WASD：ディレクショナルライトの向き（他シーンと同じ操作感）
        const float dirSpeed = 1.0f;
        if (Input::IsKeyDown('A')) m_dirX += dirSpeed * dt;
        if (Input::IsKeyDown('D')) m_dirX -= dirSpeed * dt;
        if (Input::IsKeyDown('W')) m_dirY -= dirSpeed * dt;
        if (Input::IsKeyDown('S')) m_dirY += dirSpeed * dt;
        if (m_dirX > 1.0f) m_dirX = 1.0f;
        if (m_dirX < -1.0f) m_dirX = -1.0f;
        if (m_dirY > -0.2f) m_dirY = -0.2f;  // 光が下から当たると床に影が落ちなくなるので上限を設ける
        if (m_dirY < -1.0f) m_dirY = -1.0f;

        CheckSceneNumberKeys();
    }

    void Submit(Renderer* renderer) override {
        DirectX::XMFLOAT3 dir(m_dirX, m_dirY, 0.5f);
        DirectX::XMVECTOR v = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&dir));
        DirectX::XMStoreFloat3(&dir, v);
        renderer->SetDirectionalLightDirection(dir);
        renderer->SetLightVisibilityMode(LightVisibilityMode::DirectionalOnly);
        renderer->SetDebugView(GBufferDebugView::Lit);

        // CSMの設定はBeginFrameで毎フレーム無効に戻るので、ここで毎フレーム指定する
        renderer->SetCascadedShadowEnabled(m_cascadeOn);
        renderer->SetCascadeDebug(m_cascadeDebug);
        renderer->SetCascadeLambda(m_lambda);

        // UI表示用に、前フレームで計算された境界を控えておく
        for (int i = 0; i < NUM_CASCADES; i++) m_splits[i] = renderer->GetCascadeSplit(i);

        for (auto& obj : m_objects) obj->Submit(renderer);
    }

    void SubmitUI(TextRenderer* textRenderer, float dt) override {
        textRenderer->DrawString(L"Scene11 - Cascaded Shadow Maps", 20.0f, 20.0f);
        textRenderer->DrawString(L"C : CSM On/Off   Tab : Cascade Colors", 20.0f, 80.0f);
        textRenderer->DrawString(L"N/M : Split Lambda   WASD : Move Directional Light", 20.0f, 140.0f);

        std::wstringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << (m_cascadeOn ? L"CSM : ON" : L"CSM : OFF (single 20x20 map)");
        ss << L"   Lambda = " << m_lambda;
        textRenderer->DrawString(ss.str().c_str(), 20.0f, 200.0f);

        if (m_cascadeOn) {
            std::wstringstream ss2;
            ss2 << std::fixed << std::setprecision(1);
            ss2 << L"Splits : 0 - " << m_splits[0] << L" - " << m_splits[1] << L" - " << m_splits[2] << L" m";
            textRenderer->DrawString(ss2.str().c_str(), 20.0f, 260.0f);
        }

        m_uiTime += dt;
        displayCurrentScene(textRenderer, 11);
        displayHowToUse(textRenderer, dt);
        DrawSceneNavigationHint(textRenderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;

    bool  m_cascadeOn = true;
    bool  m_cascadeDebug = false;
    float m_lambda = 0.75f;
    float m_splits[NUM_CASCADES] = {};

    float m_dirX = 0.8f;
    float m_dirY = -0.6f;
};
