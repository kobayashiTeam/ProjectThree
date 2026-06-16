// ShaderManager.h
#pragma once
#include <unordered_map>
#include "graphicsCommon.h"
#include "shader.h"

class ShaderManager {
private:
    std::unordered_map<ShaderID, Shader*> m_shaders;

    // シングルトンのお作法
    ShaderManager() = default;
    ~ShaderManager() { /* 全シェーダーの delete 処理 */ }

public:
    static ShaderManager& GetInstance() {
        static ShaderManager instance;
        return instance;
    }

    // 初期化時に、ゲームで使う全シェーダーを一括コンパイルしてしまう
    bool LoadAllShaders(ID3D11Device* pDevice) {
        // 対応表に基づいて一気に生成（内部で GetOrCreate を呼ぶ）
        if (!GetOrCreate(pDevice, ShaderID::Lit, L"Shaders/LitShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Outline, L"Shaders/OutlineShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::UnLit, L"Shaders/UnLitShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::ScreenBlit, L"Shaders/ScreenBlit.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Monochromatic, L"Shaders/Monochromatic.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Inversion, L"Shaders/InversionShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Sepia, L"Shaders/SepiaShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::SimpleBoxBlur, L"Shaders/SimpleBoxBlurShader.hlsl")) return false;
        return true;
    }

    // Enum を指定して安全にシェーダーを取り出す
    Shader* GetShader(ShaderID id) {
        auto it = m_shaders.find(id);
        if (it != m_shaders.end()) return it->second;
        return nullptr;
    }

private:
    bool GetOrCreate(ID3D11Device* pDevice, ShaderID id, const wchar_t* filename) {
        // 既存のコンパイル・生成ロジック
        Shader* pShader = new Shader();
        if (!pShader->Create(pDevice, filename, filename)) { // VS/PSが同ファイル想定
            delete pShader;
            return false;
        }
        m_shaders[id] = pShader;
        return true;
    }
};