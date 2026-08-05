#include <windows.h>
#include "game.h"
#include"input.h"

bool g_keyLeft = false;
bool g_keyRight = false;
bool g_keyUp = false;
bool g_keyDown = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (wParam == VK_LEFT)  g_keyLeft = true;
        if (wParam == VK_RIGHT) g_keyRight = true;
        if (wParam == VK_UP)    g_keyUp = true;
        if (wParam == VK_DOWN)  g_keyDown = true;
        Input::OnKeyDown(wParam);
        return 0;
    case WM_KEYUP:
        if (wParam == VK_LEFT)  g_keyLeft = false;
        if (wParam == VK_RIGHT) g_keyRight = false;
        if (wParam == VK_UP)    g_keyUp = false;
        if (wParam == VK_DOWN)  g_keyDown = false;
        //test
        Input::OnKeyUp(wParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"DX11_CleanBase";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"DirectX 11 - Clean Base",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return 0;
    ShowWindow(hWnd, nCmdShow);

    Game game;
    if (!game.Initialize(hWnd, 1280, 720)) {
        return 0;
    }

    game.Run();
    game.Shutdown();
    return 0;
}