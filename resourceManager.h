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
    //  変更：modeを追加。同じファイルでもLit用/Deferred用は別物として扱いたいので、
    //         キャッシュキーにmodeを含める（片方だけ変えても、もう片方に影響しない）
    ModelResource* GetModel(ID3D11Device* device, const std::wstring& path,
        ModelMaterialMode mode = ModelMaterialMode::Lit) {

        std::wstring key = path + (mode == ModelMaterialMode::Deferred ? L"|deferred" : L"|lit");

        auto it = m_models.find(key);
        if (it != m_models.end()) {
            return it->second; // キャッシュ済み → 再利用
        }

        ModelResource* model = new ModelResource();
        if (!model->LoadFromFile(device, &ShaderManager::GetInstance(), path, mode)) {
            delete model;
            return nullptr;
        }
        m_models[key] = model;
        return model;
    }

private:
    ResourceManager() = default;
    // 読み込んだモデルリソースをパス+モードごとに保持
    // ResourceManagerが所有する
    std::unordered_map<std::wstring, ModelResource*> m_models;
};
