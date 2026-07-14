// TestScene.h
#pragma once
#include "IScene.h"
#include"gameObject.h"
#include <vector>
#include <memory>

class GameObject;

class TestScene : public IScene {
public:
    bool Enter() override;

    void Update(float dt) override {
        for (auto& obj : m_objects) obj->Update(dt);
    }

    void Submit(Renderer* renderer) override {
        for (auto& obj : m_objects) obj->Submit(renderer);
    }

private:
    void AddObject(std::unique_ptr<GameObject> obj) {
        m_objects.push_back(std::move(obj));
    }
    std::vector<std::unique_ptr<GameObject>> m_objects;
};