#pragma once
#include <DirectXMath.h>

// シェーダーの Constant Buffer (b0) の物理的なレイアウトに完全一致させる構造体
struct PerFrameCB
{
    DirectX::XMMATRIX matView;
    DirectX::XMMATRIX matProjection;
    DirectX::XMFLOAT4 vLightPos;
    DirectX::XMFLOAT4 vLightColor;
    DirectX::XMFLOAT4 vEyePos;
    DirectX::XMFLOAT4 vAttenuation;
};

// スロット1用（オブジェクトごと）
//struct PerObjectCB
//{
//    DirectX::XMMATRIX matModel;
//};

// スロット2用（マテリアルごと・将来用）
struct PerMaterialCB
{
    // 例：XMFLOAT4 vSpecularColor; など（今回は空でも、一旦作らなくてもOK）
};