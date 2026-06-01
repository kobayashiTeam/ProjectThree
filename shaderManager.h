// ShaderManager.h
#pragma once
#include <map>
#include <string>
#include "shader.h"

class ShaderManager {
private:
    // キー：ファイル名, 値：シェーダープログラムのポインタ
    std::map<std::wstring, Shader*> m_ShaderMap;

public:
    ShaderManager() = default;
    ~ShaderManager() { Cleanup(); }

    // シェーダーを取得（なければ新規作成してキャッシュに登録）
    Shader* GetOrCreate(ID3D11Device* pDevice, const wchar_t* fileName)
    {
        std::wstring key(fileName);

        // 既にリストにあるか検索
        auto it = m_ShaderMap.find(key);
        if (it != m_ShaderMap.end())
        {
            // 発見！既存の参照（ポインタ）を貸し出す
            return it->second;
        }

        // なければ新しく作る
        Shader* pNewShader = new Shader();
        if (!pNewShader->Create(pDevice, fileName, fileName)) // VS, PS ともに同じファイルの場合
        {
            delete pNewShader;
            return nullptr;
        }

        // リスト（Dictionary）に登録して貸し出す
        m_ShaderMap[key] = pNewShader;
        return pNewShader;
    }

    void Cleanup()
    {
        for (auto& pair : m_ShaderMap)
        {
            delete pair.second; // 実体の解放
        }
        m_ShaderMap.clear();
    }
};