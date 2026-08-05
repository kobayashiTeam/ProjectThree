#pragma once
#include <DirectXMath.h>

namespace MyEngine {
    // 2“_ŠÔ‚Ì‹——£‚ğŒvZ
    inline float ComputeDistance(const DirectX::XMVECTOR& posA, const DirectX::XMVECTOR& posB) {
        DirectX::XMVECTOR dir = DirectX::XMVectorSubtract(posB, posA);
        DirectX::XMVECTOR lengthVec = DirectX::XMVector3Length(dir);
        float dist;
        DirectX::XMStoreFloat(&dist, lengthVec);
        return dist;
    }

    // üŒ`•âŠÔ
    inline float Lerp(float start, float end, float t) {
        return start + t * (end - start);
    }
}