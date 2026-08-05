#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <directxmath.h> 
#include <string>
#include <memory>
#include "shader.h"  

// ライブラリのリンク指定
#pragma comment(lib, "d3d11.lib")

// CubeMapを利用した背景描画を管理するクラス
class SkyBox {
public:

    struct SkyboxVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;   // InputLayout互換用
        DirectX::XMFLOAT4 color;    // InputLayout互換用
        DirectX::XMFLOAT2 texcoord; // InputLayout互換用
    };

    SkyBox() = default;
    ~SkyBox() = default;

    bool Initialize(ID3D11Device* device, const std::array<std::wstring, 6>& facePaths);
    bool Initialize(ID3D11Device* device, const std::wstring& ddsPath);

    void Draw(ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& viewMatrix,
        const DirectX::XMMATRIX& projectionMatrix);


private:
    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    // 描画用リソース
    ID3D11Buffer* m_pVertexBuffer = nullptr;
    ID3D11Buffer* m_indexBuffer = nullptr;
    Shader* m_shaderProgram = nullptr;
    //multiBuffer
    ID3D11Buffer* m_pPosBuffer = nullptr;
    ID3D11Buffer* m_pNrmBuffer = nullptr;
    ID3D11Buffer* m_pColorBuffer = nullptr;
    ID3D11Buffer* m_pUvBuffer = nullptr;


    // D3D11リソース & ステートオブジェクト
    ComPtr<ID3D11ShaderResourceView> m_cubeMapSRV;
    ComPtr<ID3D11RasterizerState>    m_rasterizerState;   // SkyBox描画用（Front Cull）
    ComPtr<ID3D11DepthStencilState>  m_depthStencilState; // Less_Equal用
    ComPtr<ID3D11SamplerState>       m_samplerState;      // キューブマップ用サンプラー

    // 行列をシェーダーに渡すための定数バッファ
    ComPtr<ID3D11Buffer>             m_constantBuffer;
};