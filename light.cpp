#include"light.h"

DirectX::XMMATRIX PointLight::GetViewMatrix(int face) const
{
    using namespace DirectX;

    XMFLOAT3 dirs[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    XMFLOAT3 ups[6] = {
        {0, 1, 0}, {0, 1, 0},
        {0, 0,-1}, {0, 0, 1},
        {0, 1, 0}, {0, 1, 0}
    };

    XMVECTOR pos = XMLoadFloat3(&position);
    XMVECTOR target = XMVectorAdd(pos, XMLoadFloat3(&dirs[face]));
    XMVECTOR up = XMLoadFloat3(&ups[face]);

    return XMMatrixLookAtLH(pos, target, up);
}

DirectX::XMMATRIX PointLight::GetProjectionMatrix() const
{
    // FOV90度・アスペクト1:1・nearは小さめに
    return DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XM_PIDIV2, 1.0f, 0.1f, farPlane);
}