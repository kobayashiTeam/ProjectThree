// Light.h
#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "graphicsCommon.h"

struct Light {
    LightType             type = LightType::Directional;
    DirectX::XMFLOAT3     position = { 0.0f, 5.0f, 0.0f };
    DirectX::XMFLOAT3     direction = { 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT4     color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float                 intensity = 1.0f;

    DirectX::XMMATRIX GetViewMatrix() const
    {
        using namespace DirectX;
        XMVECTOR pos = XMLoadFloat3(&position);
        XMVECTOR target = XMVectorAdd(pos, XMLoadFloat3(&direction));
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return XMMatrixLookAtLH(pos, target, up);
    }

    DirectX::XMMATRIX GetProjectionMatrix() const
    {
        using namespace DirectX;
        // 平行光源は正射影・範囲は決め打ち（後で調整）
        return XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, 50.0f);
    }
};