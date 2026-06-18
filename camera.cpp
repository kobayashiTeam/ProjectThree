#include "Camera.h"

using namespace DirectX;

Camera::Camera(float width, float height)
    : m_windowWidth(width)
    , m_windowHeight(height)
{
    //cameraの正体は場所と向きの行列。見た目的な実体はない。
    // 初期行列を設定
    m_view = XMMatrixIdentity();
    //引数内のような視錐台を仮定したとき、台内にある頂点をスクリーンに投影する
    //行列を作る関数。
    m_projection = 
        XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), width / height, 0.01f, 1000.0f);//100
    m_eyePos = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

    //yaw,pitchを初期化
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