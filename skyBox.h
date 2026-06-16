#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <directxmath.h> // XMMATRIX や XMFLOAT3 を使うために必要
#include <string>
#include <memory>

// 前回のステップで作った自作クラスのヘッダをインクルード
// (環境に合わせて実際のファイル名やパスに適宜書き換えてください)
#include "shader.h"  

// ライブラリのリンク指定
#pragma comment(lib, "d3d11.lib")

class SkyBox {
public:

    // SkyBox.h のクラス定義の上あたりに追加
    struct SkyboxVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;   // ダミー
        DirectX::XMFLOAT4 color;    // ダミー
        DirectX::XMFLOAT2 texcoord; // ダミー
    };

    SkyBox() = default;
    ~SkyBox() = default;

    // コピー禁止・ムーブ許可（リソース二重解放防止の堅牢な設計）
    SkyBox(const SkyBox&) = delete;
    SkyBox& operator=(const SkyBox&) = delete;
    SkyBox(SkyBox&&) noexcept = default;
    SkyBox& operator=(SkyBox&&) noexcept = default;

    /// <summary>
    /// スカイボックスの初期化（バッファ、シェーダー、各種ステートの生成）
    /// </summary>
    /// <param name="device">D3D11デバイスへのポインタ</param>
    /// <param name="texturePath">DDS形式のキューブマップテクスチャへのパス</param>
    /// <returns>初期化に成功したらtrue</returns>
    bool Initialize(ID3D11Device* device, const std::wstring& texturePath);

    /// <summary>
    /// スカイボックスの描画実行
    /// </summary>
    /// <param name="context">D3D11デバイスコンテキストへのポインタ</param>
    /// <param name="viewMatrix">現在のカメラのビュー行列</param>
    /// <param name="projectionMatrix">現在のカメラのプロジェクション行列</param>
    void Draw(ID3D11DeviceContext* context,
        const DirectX::XMMATRIX& viewMatrix,
        const DirectX::XMMATRIX& projectionMatrix);

private:
    // ComPtr のエイリアス（クラス内でも使いやすいように定義）
    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    // 描画用リソース
    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_indexBuffer;
    Shader* m_shaderProgram;

    // D3D11リソース & ステートオブジェクト
    ComPtr<ID3D11ShaderResourceView> m_cubeMapSRV;
    ComPtr<ID3D11RasterizerState>    m_rasterizerState;   // Cull_Front用
    ComPtr<ID3D11DepthStencilState>  m_depthStencilState; // Less_Equal用
    ComPtr<ID3D11SamplerState>       m_samplerState;      // キューブマップ用サンプラー

    // 行列をシェーダーに渡すための定数バッファ
    ComPtr<ID3D11Buffer>             m_constantBuffer;
};