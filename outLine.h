#pragma once
#include<d3d11.h>
#include"model.h"
#include"litMaterial.h"

class OutLine {
public:
	OutLine();
	~ OutLine();
	void initialize();
	void setNormalStencilState(ID3D11DepthStencilState* normal) { 
		m_pNormalStencilState = normal; }
	void setOutlineStencilState(ID3D11DepthStencilState* outline) {
		m_pOutlineStencilState = outline; }
	void setContext(ID3D11DeviceContext* context) {
		pContext = context;
	}
	void setMaterial(LitMaterial* material) {
		m_pOutlineMaterial = material;
	}
	bool createStencilState(ID3D11Device* pDevice, ID3D11DeviceContext* context);
	void DrawOutline(ID3D11DeviceContext* pContext,Model* pModel,ID3D11Buffer* pCB);
private:

	//ïœêî
public:

private:
	ID3D11DeviceContext* pContext = nullptr;
	ID3D11DepthStencilState* m_pNormalStencilState = nullptr;
	ID3D11DepthStencilState* m_pOutlineStencilState = nullptr;
	LitMaterial* m_pOutlineMaterial = nullptr;
};