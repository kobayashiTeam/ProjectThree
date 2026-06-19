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
        if (!GetOrCreate(pDevice, ShaderID::Sharpen, L"Shaders/SharpenShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Vignette, L"Shaders/VignetteShader.hlsl")) return false;
        // skybox内に追加
        if (!GetOrCreate(pDevice, ShaderID::SkyBox, L"Shaders/SkyBoxShader.hlsl")) return false;
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

        //meshは0:pos,1:normal:1,color:2,uv:3の順番でIAsetVertexBuffers登録される
        //inputLauoutも対応した番号でなければならない
        //meshで登録された情報のうち、inputLayout(shader内部)で実際に使われているものが
        //draw時にキャッシュメモリに登録される

        // IDごとにlayoutを定義
        D3D11_INPUT_ELEMENT_DESC standardLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,             D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        D3D11_INPUT_ELEMENT_DESC posOnlyLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        D3D11_INPUT_ELEMENT_DESC screenBlitLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,            D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        // IDで振り分け
        const D3D11_INPUT_ELEMENT_DESC* layout = standardLayout;
        UINT layoutCount = 4;

        switch (id) {
        case ShaderID::Lit:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::UnLit:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::Outline:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::SkyBox://今回は全対応で
            layout = posOnlyLayout;
            layoutCount = 1;
            break;
        case ShaderID::ScreenBlit:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::Monochromatic:
        case ShaderID::Inversion:
        case ShaderID::Sepia:
        case ShaderID::SimpleBoxBlur:
        case ShaderID::Sharpen:
        case ShaderID::Vignette:
        default:
            // Lit, Outline, UnLit は standardLayout
            break;
        }

        Shader* pShader = new Shader();
        if (!pShader->Create(pDevice, filename, filename, layout, layoutCount)) {
            delete pShader;
            return false;
        }
        m_shaders[id] = pShader;
        return true;
    }
};