// ResourceManager.h
#pragma once
#include <unordered_map>
#include <string>
#include<d3d11.h>
#include"ModelResource.h"
#include"shaderManager.h"


class ResourceManager {
public:
    static ResourceManager& GetInstance() {
        static ResourceManager instance;
        return instance;
    }

    // pathをキーにキャッシュ。既にあれば再利用、無ければ読み込んで登録
    ModelResource* GetModel(ID3D11Device* device, const std::wstring& path) {
        auto it = m_models.find(path);
        if (it != m_models.end()) {
            return it->second; // キャッシュ済み → 再利用
        }

        ModelResource* model = new ModelResource();
        if (!model->LoadFromFile(device, &ShaderManager::GetInstance(), path)) {
            delete model;
            return nullptr;
        }
        m_models[path] = model;
        return model;
    }

private:
    ResourceManager() = default;
    std::unordered_map<std::wstring, ModelResource*> m_models;
};