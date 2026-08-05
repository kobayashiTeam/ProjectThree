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

    ~ResourceManager()
    {
        for (auto& pair : m_models)
        {
            delete pair.second;
        }
    }

    // ファイルパスをキーとしてモデルリソースをキャッシュする
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
    // 読み込んだモデルリソースをパスごとに保持
    // ResourceManagerが所有する
    std::unordered_map<std::wstring, ModelResource*> m_models;
};