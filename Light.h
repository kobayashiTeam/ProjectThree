#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "graphicsCommon.h"


//Directional Light
struct DirectionalLight {
    LightType             type = LightType::Directional;
    DirectX::XMFLOAT3     position = { 0.0f, 10.0f, 0.0f };// シーン全体を見渡せる高さに配置
    DirectX::XMFLOAT3     direction = { 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT4     color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float                 intensity = 1.0f;

    DirectX::XMMATRIX GetViewMatrix() const
    {
        using namespace DirectX;
        XMVECTOR pos = XMLoadFloat3(&position);
        XMVECTOR target = XMVectorAdd(pos, XMLoadFloat3(&direction));

        // ライトが真下/真上を向くときはZ軸をupにする
        XMVECTOR dir = XMLoadFloat3(&direction);
        XMVECTOR up;
        float dotY = XMVectorGetY(XMVector3Dot(dir, XMVectorSet(0, 1, 0, 0)));
        if (fabsf(dotY) > 0.99f)
            up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Z軸をupに
        else
            up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        return XMMatrixLookAtLH(pos, target, up);
    }

    DirectX::XMMATRIX GetProjectionMatrix() const
    {
        using namespace DirectX;
        // 平行光源は正射影。シーンの規模に合わせて範囲を20x20に固定
        return XMMatrixOrthographicLH(20.0f, 20.0f, 0.1f, 50.0f);
    }
};



// Point Light
struct PointLight {
    LightType             type = LightType::Point;
    DirectX::XMFLOAT3 position = { 0.0f, 5.0f, 0.0f };
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float             intensity = 1.0f;
    float             farPlane = 100.0f;  // ShadowCubeMapと共有

    // 6方向それぞれのView行列
    DirectX::XMMATRIX GetViewMatrix(int face) const;
    // 透視投影・90度FOV固定
    DirectX::XMMATRIX GetProjectionMatrix() const;
};