#pragma once
#include <unordered_map>
#include "graphicsCommon.h"
#include "shader.h"
#include <wrl/client.h>

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class ShaderManager {
private:
    //描画に必須なシェーダ（VS,PS）と、geometryシェーダを分けて保管する
    std::unordered_map<ShaderID, Shader*> m_shaders;
    std::unordered_map<ShaderID, ID3D11GeometryShader*> m_geometryShaders;


    // シングルトンのお作法
    ShaderManager() = default;
    ~ShaderManager() {}

public:
    static ShaderManager& GetInstance() {
        static ShaderManager instance;
        return instance;
    }

    // 初期化時に、ゲームで使う全シェーダーを一括コンパイルしてしまう
    bool LoadAllShaders(ID3D11Device* pDevice) {
        // 使用するシェーダーを初期化時に生成する
        //basic material
        if (!GetOrCreate(pDevice, ShaderID::Lit, L"Shaders/LitShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::UnLit, L"Shaders/UnLitShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::NormalViz, L"Shaders/NormalVizShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::PointSprite, L"Shaders/PointSpriteShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::NormalMapping, L"Shaders/NormalMappingShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::ParallaxMapping, L"Shaders/ParallaxMappingShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::BasicColor, L"Shaders/BasicColorShader.hlsl")) return false;
        //screenblit
        if (!GetOrCreate(pDevice, ShaderID::ScreenBlit, L"Shaders/ScreenBlit.hlsl")) return false;
        //postProcess
        if (!GetOrCreate(pDevice, ShaderID::Monochromatic, L"Shaders/Monochromatic.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Inversion, L"Shaders/InversionShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Sepia, L"Shaders/SepiaShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::SimpleBoxBlur, L"Shaders/SimpleBoxBlurShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Sharpen, L"Shaders/SharpenShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Vignette, L"Shaders/VignetteShader.hlsl")) return false;
        // Bloom用ポストプロセス
        if (!GetOrCreate(pDevice, ShaderID::HoriBlur, L"Shaders/HorizontalBlurShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::VerBlur, L"Shaders/VerticalBlurShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::BloomCombine, L"Shaders/BloomCombineShader.hlsl")) return false;
        // skybox
        if (!GetOrCreate(pDevice, ShaderID::SkyBox, L"Shaders/SkyBoxShader.hlsl")) return false;
        //shadow
        if (!GetOrCreate(pDevice, ShaderID::Shadow, L"Shaders/ShadowShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::ShadowCube, L"Shaders/ShadowCubeShader.hlsl")) return false;
        //deferred
        if (!GetOrCreate(pDevice, ShaderID::DeferredGB, L"Shaders/DeferredGBufferShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::DeferredLighting, L"Shaders/DeferredLightingShader.hlsl")) return false;
        //SSAO
        if (!GetOrCreate(pDevice, ShaderID::SSAO, L"Shaders/SSAOShader.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::SSAOBlur, L"Shaders/SSAOBlurShader.hlsl")) return false;
        //デバッグ表示
        if (!GetOrCreate(pDevice, ShaderID::GBufferDebug, L"Shaders/GBufferDebugBlit.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::GBufferDebugDepth, L"Shaders/GBufferDebugDepthBlit.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::GBufferDebugAlpha, L"Shaders/GBufferDebugAlphaBlit.hlsl")) return false;
        return true;
    }

    // Enum を指定して安全にシェーダーを取り出す
    Shader* GetShader(ShaderID id) {
        auto it = m_shaders.find(id);
        if (it != m_shaders.end()) return it->second;
        return nullptr;
    }

    // Geometry Shader
    bool LoadAllGeometryShaders(ID3D11Device* pDevice) {
        // GSが必要なShaderIDだけここに列挙する
        if (!loadGeometryShader(pDevice, ShaderID::NormalVizGS,
            L"Shaders/NormalVizGS.hlsl")) return false;
        if (!loadGeometryShader(pDevice, ShaderID::PassThrough,
            L"Shaders/PassThroughGS.hlsl")) return false;
        if (!loadGeometryShader(pDevice, ShaderID::Move,
            L"Shaders/MoveGS.hlsl")) return false;
        if (!loadGeometryShader(pDevice, ShaderID::PointSpriteGS,
            L"Shaders/PointSpriteGS.hlsl")) return false;
        if (!loadGeometryShader(pDevice, ShaderID::ShadowCubeGS,
            L"Shaders/ShadowCubeGS.hlsl")) return false;

        return true;
    }

    ID3D11GeometryShader* getGS(ShaderID id) {
        return m_geometryShaders[id];
    }

private:
    bool GetOrCreate(ID3D11Device* pDevice, ShaderID id, const wchar_t* filename) {

        // 頂点バッファの入力順とInputLayoutのslot番号を一致させる必要がある
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

        D3D11_INPUT_ELEMENT_DESC normalMappingLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,             D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 2, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,       4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
        case ShaderID::SkyBox:
            layout = posOnlyLayout;
            layoutCount = 1;
            break;
        case ShaderID::ScreenBlit:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::NormalViz:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::PointSprite:
            layout = posOnlyLayout;
            layoutCount = 1;
            break;
        case ShaderID::Shadow:
            layout = posOnlyLayout;
            layoutCount = 1;
            break;
        case ShaderID::ShadowCube:
            layout = posOnlyLayout;
            layoutCount = 1;
            break;
        case ShaderID::NormalMapping:
            layout = normalMappingLayout;
            layoutCount = 5;
            break;
        case ShaderID::ParallaxMapping:
            layout = normalMappingLayout;
            layoutCount = 5;
            break;
        case ShaderID::HoriBlur:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::VerBlur:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::BloomCombine:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::DeferredGB:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::DeferredLighting:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::SSAO:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::SSAOBlur:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::GBufferDebug:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::GBufferDebugDepth:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::GBufferDebugAlpha:
            layout = screenBlitLayout;
            layoutCount = 2;
            break;
        case ShaderID::BasicColor:
            layout = standardLayout;
            layoutCount = 4;
            break;
        case ShaderID::Monochromatic:
        case ShaderID::Inversion:
        case ShaderID::Sepia:
        case ShaderID::SimpleBoxBlur:
        case ShaderID::Sharpen:
        case ShaderID::Vignette:
        default:
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


    bool loadGeometryShader(ID3D11Device* pDevice, ShaderID id, const wchar_t* filename) {
        ComPtr<ID3DBlob> blob, errBlob;
        HRESULT hr = D3DCompileFromFile(
            filename, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "GSmain", "gs_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0, &blob, &errBlob
        );
        if (FAILED(hr)) {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return false;
        }
        ID3D11GeometryShader* gs = nullptr;
        hr = pDevice->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &gs);
        if (FAILED(hr)) return false;
        m_geometryShaders[id] = gs;
        return true;
    }



};