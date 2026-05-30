#include "Camera.h"

using namespace DirectX;

Camera::Camera(float width, float height)
    : m_windowWidth(width)
    , m_windowHeight(height)
{
    // èâä˙çsóÒÇê›íË
    m_view = XMMatrixIdentity();
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