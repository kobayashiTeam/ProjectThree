// game.h
#pragma once
#include <windows.h>
#include <memory>

#include"iScene.h"

class Graphics;
class Renderer;
class Camera;
class TextRenderer;

class Game {
public:
    bool Initialize(HWND hWnd, UINT width, UINT height);
    void Run();
    void Shutdown();

private:
    void Update(float dt);
    void Render(float dt);

    Graphics* m_graphics = nullptr;
    Renderer* m_renderer = nullptr;
    Camera* m_camera = nullptr;
    TextRenderer* m_textRenderer = nullptr;
    std::unique_ptr<IScene> m_currentScene;
};