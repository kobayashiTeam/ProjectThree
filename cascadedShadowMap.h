#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "graphicsCommon.h"

template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

// カスケードシャドウマップの「入れ物」だけを担当するクラス
// 2048x2048の深度テクスチャをNUM_CASCADES枚重ねたTexture2DArrayとして持つ
//   ・書き込み：段ごとに別々のDSV（配列の1スライスだけを指すビュー）
//   ・読み取り：配列全体を1つのSRVとして渡し、シェーダー側でスライス番号を指定する
// 行列の計算（どこに方眼紙を敷くか）はShadowSystem側の責務で、このクラスは知らない
class CascadedShadowMap {
public:
    bool Initialize(ID3D11Device* device, UINT size);

    void BeginRender(ID3D11DeviceContext* ctx, int cascadeIndex); // 指定スライスをDSVにセット＋クリア
    void EndRender(ID3D11DeviceContext* ctx);

    ID3D11ShaderResourceView* GetSRV() const { return m_srv.Get(); }
    UINT GetSize() const { return m_size; }

private:
    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11DepthStencilView>   m_dsv[NUM_CASCADES];
    ComPtr<ID3D11ShaderResourceView> m_srv;
    UINT m_size = 0;
};
