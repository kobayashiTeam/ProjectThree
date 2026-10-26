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
    // CSはVS/PSと組にならない単独のシェーダーなので、GSと同じく別に保管する
    std::unordered_map<ShaderID, ComPtr<ID3D11ComputeShader>> m_computeShaders;


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
        // Bloom（縮小／拡大の連鎖）とトーンマッピング
        if (!GetOrCreate(pDevice, ShaderID::BloomDownsample, L"Shaders/BloomDownsample.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::BloomUpsample, L"Shaders/BloomUpsample.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::Tonemap, L"Shaders/Tonemap.hlsl")) return false;
        // skybox
        if (!GetOrCreate(pDevice, ShaderID::SkyBox, L"Shaders/SkyBoxShader.hlsl")) return false;
        //IBL
        if (!GetOrCreate(pDevice, ShaderID::IrradianceConvolution, L"Shaders/IrradianceConvolution.hlsl")) return false;
        if (!GetOrCreate(pDevice, ShaderID::PrefilterSpecular, L"Shaders/PrefilterSpecular.hlsl")) return false;
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
        //GPUパーティクル（VS/PS。GSは LoadAllGeometryShaders で同じファイルから読む）
        if (!GetOrCreate(pDevice, ShaderID::ParticleDraw, L"Shaders/ParticleDraw.hlsl")) return false;
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
        // パーティクルは最大100万個の点を通すので、最適化を効かせる（計測にも使うため）
        if (!loadGeometryShader(pDevice, ShaderID::ParticleDrawGS,
            L"Shaders/ParticleDraw.hlsl", true)) return false;

        return true;
    }

    ID3D11GeometryShader* getGS(ShaderID id) {
        return m_geometryShaders[id];
    }

    // Compute Shader（エントリポイントは既定で CSMain。1つのファイルに複数あるときは名前を渡す）
    bool LoadAllComputeShaders(ID3D11Device* pDevice) {
        if (!loadComputeShader(pDevice, ShaderID::InversionCS, L"Shaders/InversionCS.hlsl")) return false;
        if (!loadComputeShader(pDevice, ShaderID::BoxBlurCS, L"Shaders/BoxBlurCS.hlsl")) return false;
        if (!loadComputeShader(pDevice, ShaderID::BoxBlurCSNoShared, L"Shaders/BoxBlurCS.hlsl", "CSMainNoShared")) return false;
        if (!loadComputeShader(pDevice, ShaderID::ParticleUpdateCS, L"Shaders/ParticleUpdateCS.hlsl")) return false;
        return true;
    }

    ID3D11ComputeShader* GetCS(ShaderID id) {
        auto it = m_computeShaders.find(id);
        return (it != m_computeShaders.end()) ? it->second.Get() : nullptr;
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
        case ShaderID::IrradianceConvolution:
            layout = posOnlyLayout;
            layoutCount = 1;
            break;
        case ShaderID::PrefilterSpecular:
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
        case ShaderID::BloomDownsample:
        case ShaderID::BloomUpsample:
        case ShaderID::Tonemap:
            layout = screenBlitLayout;
            layoutCount = 2;
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
        case ShaderID::ParticleDraw:
            // 頂点バッファから何も読まない（VSは SV_VertexID だけを使う）ので、InputLayout を作らない
            layout = nullptr;
            layoutCount = 0;
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


    // optimize：既定（false）はデバッグ用に最適化を切る。計測に使うGSは true にしてPS/CSと同じ条件にする
    bool loadGeometryShader(ID3D11Device* pDevice, ShaderID id, const wchar_t* filename,
        bool optimize = false) {
        ComPtr<ID3DBlob> blob, errBlob;
        HRESULT hr = D3DCompileFromFile(
            filename, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            "GSmain", "gs_5_0",
            optimize ? 0 : (D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION),
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

    // 最適化のフラグはPS（Shader::Create）と同じ既定（0）にそろえる
    // （GSのようにデバッグ用に最適化を切ると、PS版とのmsの比べ合いが公平でなくなる）
    bool loadComputeShader(ID3D11Device* pDevice, ShaderID id, const wchar_t* filename,
        const char* entryPoint = "CSMain") {
        ComPtr<ID3DBlob> blob, errBlob;
        HRESULT hr = D3DCompileFromFile(
            filename, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint, "cs_5_0",
            0, 0, &blob, &errBlob
        );
        if (FAILED(hr)) {
            if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
            return false;
        }
        ComPtr<ID3D11ComputeShader> cs;
        hr = pDevice->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &cs);
        if (FAILED(hr)) return false;
        m_computeShaders[id] = cs;
        return true;
    }



};
