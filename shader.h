// Shader.h
#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>

class Shader {
private:
    ID3D11VertexShader* m_pVertexShader = nullptr;
    ID3D11PixelShader* m_pPixelShader = nullptr;
    ID3D11InputLayout* m_pVertexLayout = nullptr;

public:
    Shader() = default;
    ~Shader() { Cleanup(); }

    // 生成メソッド
    bool Create(ID3D11Device* pDevice, 
        const wchar_t* vsFileName, 
        const wchar_t* psFileName,
        const D3D11_INPUT_ELEMENT_DESC* layout,  // 追加
        UINT layoutCount                          // 追加)
        );

    // 利用（バインド）メソッド
    void Bind(ID3D11DeviceContext* pContext);

    void Cleanup();
};