#include"scene0.h"
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

bool Scene0::Enter(){
	return true;
}

//bool Scene0::Enter() {
//	// ★今main.cppでやっているモデル生成＋AddObjectを、ここに全部移す
//	// 例:
//	// auto obj = std::make_unique<GameObject>();
//	// AddObject(std::move(obj));
//
//	ModelResource* modelResource = nullptr;
//
//	//宣言
//		//mesh
//	Mesh* g_pCubeMesh = nullptr;
//	//material
//	LitMaterial* g_pLitMaterial = nullptr;
//	UnLitMaterial* g_pUnLitMaterial = nullptr;
//	OutLineMaterial* g_pOutlineMaterial = nullptr;
//	NormalVizMaterial* g_pNormalVizMaterial = nullptr;
//	NormalMappingMaterial* g_pNormalMappingMaterial = nullptr;
//	LitMaterial* g_pTestLitMaterial = nullptr;
//	ParallaxMappingMaterial* g_pParallaxMappingMaterial = nullptr;
//	DeferredCBMaterial* g_pDeferredCBMaterial = nullptr;
//	//model
//	Model* g_pMainModel = nullptr;//宙に浮かぶ立体cube
//	Model* g_pMainModel2 = nullptr;//transparent cube
//	Model* g_pMainModel3 = nullptr;//土台cube
//	Model* g_pMainModel4 = nullptr;//brick
//	Model* g_pMainModel5 = nullptr;//parallax
//	Model* g_pOldCameraBagModel = nullptr;//oldCamera(modelResource)用モデル
//	//GSEffect
//	MoveGSEffect* g_pMoveGSEffect = nullptr;
//	NormalVizGSEffect* g_pNormalVizGSEffect = nullptr;
//	//Outline
//	OutLine* g_pOutLine = nullptr;
//
//
//	//中身の生成と代入
//	//GS
//		//moveGS
//	g_pMoveGSEffect = new MoveGSEffect();
//	g_pMoveGSEffect->Initialize(m_device, ShaderManager::GetInstance().getGS(ShaderID::Move));
//	//normalVizGS
//	g_pNormalVizGSEffect = new NormalVizGSEffect();
//	g_pNormalVizGSEffect->Initialize(m_device, ShaderManager::GetInstance().getGS(ShaderID::NormalVizGS));
//	g_pNormalVizGSEffect->SetNormalParams(0.1, 0, 0, 0);
//
//	// Mesh作成（Cube）
//	g_pCubeMesh = Mesh::CreateCube(m_device, 1);
//
//	// Material
//		//texture
//	UINT32 checker[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
//	UINT32 white = 0xFFFFFFFF;
//	//HDR用の白（各チャンネル 1.0f の輝度）
//	DirectX::PackedVector::XMHALF4 whiteHDR(1.0f, 1.0f, 1.0f, 1.0f);
//	//litMaterial
//	g_pLitMaterial = new LitMaterial();
//	if (!g_pLitMaterial->Initialize(m_device, ShaderManager::GetInstance().
//		GetShader(ShaderID::Lit), &whiteHDR, 1, 1, true, true)) return false;
//	g_pLitMaterial->CreateMaterialBuffer(m_device);
//	g_pLitMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
//	//g_pLitMaterial->SetGSEffect(g_pMoveGSEffect);
//	//g_pMoveGSEffect->SetOffset(0.0f,1.0f,0.0f);
//		//unLitMaterial
//	g_pUnLitMaterial = new UnLitMaterial();
//	if (!g_pUnLitMaterial->Initialize(m_device, ShaderManager::GetInstance().
//		GetShader(ShaderID::UnLit), checker, 2, 2, true, false))//unlit
//		return false;
//	g_pUnLitMaterial->CreateMaterialBuffer(m_device);
//	g_pUnLitMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 0.3f); // 緑がかった色で描画
//	//g_pUnLitMaterial->SetGS(ShaderManager::GetInstance().getGS(ShaderID::PassThrough));
//		//outlienMaterial
//	g_pOutlineMaterial = new OutLineMaterial();
//	if (!g_pOutlineMaterial->Initialize(m_device, ShaderManager::GetInstance().
//		GetShader(ShaderID::Outline), checker, 2, 2, true, false))return false;
//	g_pOutlineMaterial->CreateMaterialBuffer(m_device);
//	g_pOutlineMaterial->SetMaterialColor(1.0f, 0.0f, 0.0f, 1.0f); // 赤色で描画
//	//normalvizMaterial
//	g_pNormalVizMaterial = new NormalVizMaterial();
//	if (!g_pNormalVizMaterial->Initialize(m_device, ShaderManager::GetInstance().
//		GetShader(ShaderID::NormalViz), checker, 2, 2, true, true)) return false;
//	g_pNormalVizMaterial->SetGSEffect(g_pNormalVizGSEffect);
//	//normalMappingMaterial
//	g_pNormalMappingMaterial = new NormalMappingMaterial();
//	//まずdiffuse画像を設定
//	if (!g_pNormalMappingMaterial->InitializeFromFile(m_device,
//		ShaderManager::GetInstance().GetShader(ShaderID::NormalMapping),
//		L"assets/nor/diff.png"))return false;
//	//次にnormal画像を設定
//	if (!g_pNormalMappingMaterial->InitializeNormalMapFromFile(m_device,
//		L"assets/nor/nor.png"))return false;
//	//testLitMaterial
//	g_pTestLitMaterial = new LitMaterial();
//	if (!g_pTestLitMaterial->InitializeFromFile(m_device,
//		ShaderManager::GetInstance().GetShader(ShaderID::Lit),
//		L"assets/nor/diff.png"))return false;
//	//parallax
//	g_pParallaxMappingMaterial = new ParallaxMappingMaterial();
//	if (!g_pParallaxMappingMaterial->InitializeFromFile(m_device,
//		ShaderManager::GetInstance().GetShader(ShaderID::ParallaxMapping),
//		L"assets/para/diff.png"))return false;
//	//次にnormal画像を設定
//	if (!g_pParallaxMappingMaterial->InitializeParallaxMapFromFile(m_device,
//		L"assets/para/normalHeight.png"))return false;
//	//deferredCBMaterial
//	g_pDeferredCBMaterial = new DeferredCBMaterial();
//	if (!g_pDeferredCBMaterial->Initialize(m_device, ShaderManager::
//		GetInstance().GetShader(ShaderID::DeferredGB), &whiteHDR, 1, 1, true, true)) return false;
//	g_pDeferredCBMaterial->CreateMaterialBuffer(m_device);
//	g_pDeferredCBMaterial->SetMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
//
//
//	// Model
//		//Model1
//	g_pMainModel = new Model(m_device, g_pCubeMesh, g_pDeferredCBMaterial);//deferredへ
//	g_pMainModel->SetPosition(0.0f, -0.5f, 3.0f);
//	//Model2
//	g_pMainModel2 = new Model(m_device, g_pCubeMesh, g_pUnLitMaterial);
//	g_pMainModel2->SetPosition(0.0f, 0.0f, 0.0f);
//	g_pMainModel2->SetTransparent(true);
//	//Model3
//	g_pMainModel3 = new Model(m_device, g_pCubeMesh, g_pDeferredCBMaterial);//deferredへ
//	g_pMainModel3->SetPosition(0.0f, -6.2f, 5.0f);//y-7
//	g_pMainModel3->SetScale(10.0f, 10.0f, 10.0f);
//	//Model4
//	g_pMainModel4 = new Model(m_device, g_pCubeMesh, g_pNormalMappingMaterial);
//	g_pMainModel4->SetPosition(-3.0f, 0.0f, -3.0f);
//	g_pMainModel4->SetScale(3.0f, 3.0f, 3.0f);
//	//Model5
//	g_pMainModel5 = new Model(m_device, g_pCubeMesh, g_pParallaxMappingMaterial);
//	g_pMainModel5->SetPosition(3.0f, 0.0f, -3.0f);
//	g_pMainModel5->SetScale(3.0f, 3.0f, 3.0f);
//	//oldCamera(modelResource)
//	modelResource = new ModelResource();
//	modelResource = ResourceManager::GetInstance().GetModel(m_device, L"assets/oldCamera/scene.gltf");
//	g_pOldCameraBagModel = new Model(m_device, modelResource);
//	g_pOldCameraBagModel->SetPosition(-3.0f, 0.0f, 20.0f);//5,0,-3
//
//	//メンバ配列にこれらのモデルを登録
//	AddObject(std::make_unique<GameObject>(g_pMainModel, RenderPass::DeferredOpaque, BlendMode::Opaque));
//	AddObject(std::make_unique<GameObject>(g_pMainModel2, RenderPass::Transparent, BlendMode::AlphaBlend));
//	AddObject(std::make_unique<GameObject>(g_pOldCameraBagModel, RenderPass::Opaque, BlendMode::Opaque));
//	AddObject(std::make_unique<GameObject>(g_pMainModel4, RenderPass::Opaque, BlendMode::Opaque));
//	AddObject(std::make_unique<GameObject>(g_pMainModel5, RenderPass::Opaque, BlendMode::Opaque));
//	//maimodel3も入れる
//	AddObject(std::make_unique<GameObject>(g_pMainModel3, RenderPass::DeferredOpaque, BlendMode::Opaque));
//
//
//	return true;
//}