// MathUtils.h
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

    // ‚Â‚¢‚Å‚É«—ˆg‚¢‚»‚¤‚È‚à‚Ì‚à‚±‚±‚É“ü‚ê‚Ä‚¢‚­
    inline float Lerp(float start, float end, float t) {
        return start + t * (end - start);
    }
}