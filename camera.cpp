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
        XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), width / height, 0.01f, 100.0f);
    m_eyePos = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
}

Camera::~Camera()
{
}

void Camera::Update(XMVECTOR eye, XMVECTOR at, XMVECTOR up)
{
    m_view = XMMatrixLookAtLH(eye, at, up);
    XMStoreFloat4(&m_eyePos, eye);
}