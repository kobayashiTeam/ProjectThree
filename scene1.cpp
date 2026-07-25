#include"scene1.h"
#include <DirectXPackedVector.h> // 必要に応じてインクルード 新規

#include"litMaterial.h"
#include"unLitMaterial.h"
#include"normalVizMaterial.h"
#include"normalMappingMaterial.h"
#include"parallaxMappingMaterial.h"
#include"deferredCBMaterial.h"
#include"outLineMaterial.h"
#include"mesh.h"
#include"resourceManager.h"
#include"outLine.h"

#include"moveGSEffect.h"
#include"normalVizGSEffect.h"

#include"shaderManager.h"

bool Scene1::Enter() {
	
	ModelResource* modelResource = nullptr;

	//宣言
		//mesh
	Mesh* g_pCubeMesh = nullptr;
	//material
	LitMaterial* g_pLitMaterial = nullptr;
	//model
	Model* g_pMainModel = nullptr;//宙に浮かぶ立体cube
	

	// Mesh作成（Cube）
	g_pCubeMesh = Mesh::CreateCube(m_device, 1);

	// Material
		//texture
	UINT32 checker[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
	UINT32 white = 0xFFFFFFFF;
	//HDR用の白（各チャンネル 1.0f の輝度）
	DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
	//litMaterial
	g_pLitMaterial = new LitMaterial();
	if (!g_pLitMaterial->Initialize(m_device, ShaderManager::GetInstance().
		GetShader(ShaderID::BasicColor), &whiteHDR, 1, 1, true, true)) return false;
	g_pLitMaterial->CreateMaterialBuffer(m_device);
	g_pLitMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
		//unLitMaterial
	// Model
		//Model1
	g_pMainModel = new Model(m_device, g_pCubeMesh, g_pLitMaterial);
	g_pMainModel->SetScale(2,2,2);
	g_pMainModel->SetPosition(0.0f, -0.5f, 3.0f);
	g_pMainModel->SetRotation(0.5f, 0.5f, 0.0f);
	
	//メンバ配列にこれらのモデルを登録
	AddObject(std::make_unique<GameObject>(g_pMainModel, RenderPass::Opaque, BlendMode::Opaque));
	
	return true;
}