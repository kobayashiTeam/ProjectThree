// input.cpp
#include "input.h"

bool Input::s_currKeys[256] = {};
bool Input::s_prevKeys[256] = {};

void Input::OnKeyDown(unsigned int vkCode)
{
    if (vkCode < 256) s_currKeys[vkCode] = true;
}

void Input::OnKeyUp(unsigned int vkCode)
{
    if (vkCode < 256) s_currKeys[vkCode] = false;
}

void Input::Update()
{
    // 「前フレーム」に「今フレーム」の状態をコピーしてから、
    // 次のOnKeyDown/OnKeyUpで「今フレーム」を更新していく
    for (int i = 0; i < 256; ++i)
    {
        s_prevKeys[i] = s_currKeys[i];
    }
}

bool Input::IsKeyDown(int vkCode)
{
    if (vkCode < 0 || vkCode >= 256) return false;
    return s_currKeys[vkCode];
}

bool Input::IsKeyPressed(int vkCode)
{
    if (vkCode < 0 || vkCode >= 256) return false;
    return s_currKeys[vkCode] && !s_prevKeys[vkCode];
}