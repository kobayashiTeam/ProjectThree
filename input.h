// input.h
// キーボード入力の一元管理。static メンバのみで完結する（インスタンス化しない）。
// WndProcは生の状態を書き込むだけにして、各Scene/カメラはここ経由で問い合わせる。
#pragma once

class Input
{
public:
    // WndProcから呼ぶ。生のキー状態を記録するだけ
    static void OnKeyDown(unsigned int vkCode);
    static void OnKeyUp(unsigned int vkCode);

    // Game::Update()の先頭、各Scene::Update()より前に1回だけ呼ぶこと
    static void Update();

    // 押している間ずっとtrue（カメラ移動など）
    static bool IsKeyDown(int vkCode);

    // 押された瞬間の1フレームだけtrue（シーン切り替えなど）
    static bool IsKeyPressed(int vkCode);

private:
    static bool s_currKeys[256];
    static bool s_prevKeys[256];
};