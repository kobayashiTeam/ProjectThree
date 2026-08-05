#pragma once
#include <DirectXMath.h>

class Camera
{
public:
    Camera(float width, float height);
    ~Camera();

    // 毎フレーム更新（必要に応じてカメラの位置を動かせるようにする）
    void Update(DirectX::XMVECTOR eye, DirectX::XMVECTOR at, DirectX::XMVECTOR up);

    // 定数バッファへ渡すためのアクセサ
    DirectX::XMMATRIX GetViewMatrix() const { return m_view; }
    DirectX::XMMATRIX GetProjectionMatrix() const { return m_projection; }
    DirectX::XMFLOAT4 GetEyePosition() const { return m_eyePos; }
    void UpdateDirection(float deltaYaw, float deltaPitch);

private:
    DirectX::XMMATRIX m_view;
    DirectX::XMMATRIX m_projection;
    DirectX::XMFLOAT4 m_eyePos;

    float m_windowWidth;
    float m_windowHeight;

    float m_yaw;// 左右（ラジアン）
    float m_pitch;  // 上下（ラジアン）
};