#include "iScene.h"
#include "input.h"
#include "scene0.h"
#include "scene1.h"
#include "scene2.h"
#include "scene3.h"
#include "scene4.h"
#include "scene5.h"
#include"scene6.h"
#include"scene7.h"
#include"scene8.h"
#include"scene9.h"
#include"scene10.h"

void IScene::CheckSceneNumberKeys() {
    if (Input::IsKeyPressed('0')) { m_nextScene = new Scene0(); return; }
    if (Input::IsKeyPressed('1')) { m_nextScene = new Scene1(); return; }
    if (Input::IsKeyPressed('2')) { m_nextScene = new Scene2(); return; }
    if (Input::IsKeyPressed('3')) { m_nextScene = new Scene3(); return; }
    if (Input::IsKeyPressed('4')) { m_nextScene = new Scene4(); return; }
    if (Input::IsKeyPressed('5')) { m_nextScene = new Scene5(); return; }
    if (Input::IsKeyPressed('6')) { m_nextScene = new Scene6(); return; }
    if (Input::IsKeyPressed('7')) { m_nextScene = new Scene7(); return; }
    if (Input::IsKeyPressed('8')) { m_nextScene = new Scene8(); return; }
    if (Input::IsKeyPressed('9')) { m_nextScene = new Scene9(); return; }
    // ★Scene10は0〜9の数字を使い切っているのでF1に割り当て
    if (Input::IsKeyPressed(VK_F1)) { m_nextScene = new Scene10(); return; }
}

void IScene::DrawSceneNavigationHint(TextRenderer* textRenderer) {
    textRenderer->DrawString(
        L"0:Index 1:Base 2:Texture&Gamma 3:Deferred 4:NormalMap",
        500.0f, 640.0f,           // 画面右下寄り（1280x720基準）
        1.0f, 1.0f, 1.0f, 1.0f,   // 白色・不透明で表示
        0.7f);                    // scale：本文より小さく表示
    textRenderer->DrawString(
        L"5:Shadow 6:HDR+Bloom 7:GPU Instancing 8:All 9:PBR Grid F1:PBR Interactive",
        500.0f, 660.0f,           // 画面右下寄り（1280x720基準）
        1.0f, 1.0f, 1.0f, 1.0f,   // 白色・不透明で表示
        0.7f);                    // scale：本文より小さく表示
}

void IScene::displayCurrentScene(TextRenderer* textRenderer, int current) {
    int posX = UILayoutCommon::currentScenePositionX;
    int posY = UILayoutCommon::currentScenePositionY;

    std::wstring text = L"Scene" + std::to_wstring(current) + L"/10";

    textRenderer->DrawString(text.c_str(), posX, posY);

}

void IScene::displayHowToUse(TextRenderer* textRenderer, float dt) {
    int posX = UILayoutCommon::howToUsePositionX;
    int posY = UILayoutCommon::howToUsePositionY;
    int spaceY = UILayoutCommon::howToUseSpaceY;
    float speed = 3.0f;
    float alpha = (sinf(m_uiTime * speed) + 2.0f) * 0.5f;
    textRenderer->DrawString(L"Jump to Scene:Press Number Key", posX, posY,
        1.0f, 1.0f, 1.0f, alpha);
}
