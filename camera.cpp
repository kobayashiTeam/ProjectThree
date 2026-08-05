#include "Camera.h"

using namespace DirectX;

Camera::Camera(float width, float height)
    : m_windowWidth(width)
    , m_windowHeight(height)
{
    // ビュー行列はカメラの位置と向きを表現する行列
    m_view = XMMatrixIdentity();
    // 透視投影行列を生成
    m_projection = 
        XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), width / height, 0.01f, 1000.0f);
    m_eyePos = XMFLOAT4(0.0f, 0.0f,-10.0f, 0.0f);

    // カメラの向きを初期化
    m_yaw = XMConvertToRadians(90.0f);  // +Z方向;    // 左右（ラジアン）
    m_pitch=0.0f;  // 上下（ラジアン）
}

Camera::~Camera()
{
}

void Camera::Update(XMVECTOR eye, XMVECTOR at, XMVECTOR up)
{
    m_view = XMMatrixLookAtLH(eye, at, up);
    XMStoreFloat4(&m_eyePos, eye);
}

void Camera::UpdateDirection(float deltaYaw, float deltaPitch)
{
    m_yaw += deltaYaw;
    m_pitch += deltaPitch;

    // pitchの上限（真上・真下を超えないように）
    const float limit = XMConvertToRadians(89.0f);
    if (m_pitch > limit) m_pitch = limit;
    if (m_pitch < -limit) m_pitch = -limit;

    // yaw/pitchからforward方向ベクトルを計算
    XMVECTOR forward = XMVectorSet(
        cosf(m_pitch) * cosf(m_yaw),   // X
        sinf(m_pitch),                  // Y
        cosf(m_pitch) * sinf(m_yaw),   // Z
        0.0f
    );

    XMVECTOR eye = XMVectorSet(m_eyePos.x, m_eyePos.y, m_eyePos.z, 0.0f);
    XMVECTOR at = XMVectorAdd(eye, forward);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    m_view = XMMatrixLookAtLH(eye, at, up);
}