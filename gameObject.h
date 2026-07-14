// GameObject.h
#pragma once
#include "graphicsCommon.h" // RenderPass, BlendMode
#include"renderer.h"

class Model;

class GameObject {
public:
    GameObject(Model* model, RenderPass pass, BlendMode mode)
        : m_model(model), m_renderPass(pass), m_blendMode(mode) {
    }
    virtual ~GameObject() = default;

    // ゲームロジック側。派生クラス（Player/Enemyなど）がオーバーライドする
    virtual void Update(float dt) {}

    // 描画側。今はほぼ全GameObjectで共通なので基底クラスで完結できる
    virtual void Submit(Renderer* renderer) {
        if (m_model) renderer->Submit(m_model, m_renderPass, m_blendMode);
    }

    Model* GetModel() const { return m_model; }

protected:
    Model* m_model = nullptr; // 借用。所有権はResourceManager/呼び出し元
    RenderPass m_renderPass;
    BlendMode m_blendMode;
};