#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>

// DirectX11のシェーダーと入力レイアウトを管理するクラス
class Shader {
private:
    ID3D11VertexShader* m_pVertexShader = nullptr;
    ID3D11PixelShader* m_pPixelShader = nullptr;
    ID3D11InputLayout* m_pVertexLayout = nullptr;
    ID3D11GeometryShader* m_pGeometryShader = nullptr;

public:
    Shader() = default;
    ~Shader() { Cleanup(); }

    // HLSLファイルからシェーダーを生成する
    bool Create(ID3D11Device* pDevice, 
        const wchar_t* vsFileName, 
        const wchar_t* psFileName,
        const D3D11_INPUT_ELEMENT_DESC* layout, 
        UINT layoutCount                        
        );

    // 利用（バインド）メソッド
    void Bind(ID3D11DeviceContext* pContext);

    void Cleanup();

    // ジオメトリシェーダーを追加設定する
    void SetGS(ID3D11GeometryShader* gs) { m_pGeometryShader = gs; }
};