// GSEffect.h
#include<d3d11.h>

class GSEffect {
public:
    virtual ~GSEffect() = default;
    virtual void Bind(ID3D11DeviceContext* pContext) = 0;
    // cbuffer更新はBindの中でやるか、Update()を別に用意するかは好み
protected:
    ID3D11GeometryShader* m_pGS = nullptr;
    ID3D11Buffer* m_pCB = nullptr;
};