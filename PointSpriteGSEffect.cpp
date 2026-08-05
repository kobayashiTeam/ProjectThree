#include"pointSpriteGSEffect.h"
#include"shader.h"
#include"shaderManager.h"

bool PointSpriteGSEffect::Init(ID3D11Device* pDevice) {

	HRESULT hr;

	//VS,PS,GS設定
	setShader(ShaderManager::GetInstance().GetShader(ShaderID::PointSprite));
	setGS(ShaderManager::GetInstance().getGS (ShaderID::PointSpriteGS));
	if (!m_pGS || !m_pShader)return false;

	//頂点生成
	DirectX::XMFLOAT3 positions[] = {
		{0.0f,5.0f,3.0f}
	};
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};

	bd.ByteWidth = sizeof(DirectX::XMFLOAT3) * _countof(positions);//vertexCount
	initData.pSysMem = positions;
	if (FAILED(pDevice->CreateBuffer(&bd, &initData, &m_vb)))   return false;
	if (!m_vb)return false;

	//cbを2つ生成
	// モデル用CBの生成
	bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(PerObjectCB);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr=pDevice->CreateBuffer(&bd, nullptr, &m_cbModelb);
	if (FAILED(hr))return false;

	// スプライト用CBの生成
	bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(PerSpriteCB);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = pDevice->CreateBuffer(&bd, nullptr, &m_cbSpriteb);
	if (FAILED(hr))return false;

	if (!m_cbModelb || !m_cbSpriteb)return false;

	//初期化
	m_cbModelData.mModel = DirectX::XMMatrixIdentity(); // 単位行列で初期化

	m_cbSpriteData.SpriteSize = 0.3f;                          // 0より大きい値
	m_cbSpriteData.SpriteColor = DirectX::XMFLOAT3(1, 1, 0);

	return true;
}

void PointSpriteGSEffect::Bind(ID3D11DeviceContext* pContext) {
	// このエフェクトはDraw内でシェーダ・バッファのバインドを完結させるため、Bindでは何もしない
}

void PointSpriteGSEffect::Draw(ID3D11DeviceContext* pContext) {
	// トポロジをPointListに変更
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	//シェーダバインド(layout,vs,psをバインド)
	m_pShader->Bind(pContext);
	//gsバインド
	pContext->GSSetShader(m_pGS, nullptr, 0);
	//頂点情報のバインド
	ID3D11Buffer* vbs[1] = { m_vb.Get()};
	UINT strides[1] = {sizeof(DirectX::XMFLOAT3)};
	UINT offsets[1] = { 0};
	pContext->IASetVertexBuffers(0, 1, vbs, strides, offsets);
	//インデックスバインド:今回は何もなし
	// cbuffer
		//model
	D3D11_MAPPED_SUBRESOURCE mapped;
	pContext->Map(m_cbModelb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, &m_cbModelData, sizeof(PerObjectCB));
	pContext->Unmap(m_cbModelb.Get(), 0);
		//送る
	ID3D11Buffer* cb = m_cbModelb.Get();
	pContext->VSSetConstantBuffers(1, 1, &cb);
	cb = nullptr;
	//sprite
	pContext->Map(m_cbSpriteb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, &m_cbSpriteData, sizeof(PerSpriteCB));
	pContext->Unmap(m_cbSpriteb.Get(), 0);
	//送る
	cb = m_cbSpriteb.Get();
	pContext->GSSetConstantBuffers(3, 1, &cb);//3
	cb = nullptr;

	//描画
	pContext->Draw(1, 0);

	// PointListはこの描画専用のため、描画後はTriangleListに戻す
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}