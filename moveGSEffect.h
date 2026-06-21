// MoveGSEffect.h
#include"gsEffect.h"

class MoveGSEffect : public GSEffect {
private:
    // このクラス専用のcb構造体
    struct CBData {
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float offsetZ = 0.0f;
        float _pad = 0.0f;
    } m_cbData;  // 構造体の実体をメンバとして持つ

public:
    bool Initialize(ID3D11Device* pDevice, ID3D11GeometryShader* pGS) {
        m_pGS = pGS;

        // このクラス専用のcbufferを作る
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(CBData);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(pDevice->CreateBuffer(&bd, nullptr, &m_pCB));
    }

    // 外から値を変えたいときの専用セッター
    void SetOffset(float x, float y, float z) {
        m_cbData.offsetX = x;
        m_cbData.offsetY = y;
        m_cbData.offsetZ = z;
    }

    void Bind(ID3D11DeviceContext* pContext) override {
        // cbufferの中身を更新してGPUに送る
        D3D11_MAPPED_SUBRESOURCE mapped;
        pContext->Map(m_pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &m_cbData, sizeof(CBData));
        pContext->Unmap(m_pCB, 0);

        // GSとcbufferをバインド
        pContext->GSSetShader(m_pGS, nullptr, 0);
        pContext->GSSetConstantBuffers(3, 1, &m_pCB);//3
    }
};