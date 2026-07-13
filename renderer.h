#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "rasterizerStates.h"
#include "graphicsCommon.h"
#include"renderTarget.h"

#pragma comment(lib, "d3d11.lib")
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

//実体宣言
#include"renderQueue.h"
#include"light.h"
#include"shadowMap.h"
#include"shadowCubeMap.h"

//前方宣言
class Graphics;
class Camera;
class Model;
class DepthStencilStates;
class BlendStates;
class ScreenBlitMaterial;
class ScreenBlitPostProcess;
class MonochromePostProcess;
class PostProcess;
class InversionPostProcess;
class SepiaPostProcess;
class SimpleBoxBlurPostProcess;
class SharpenPostProcess;
class VignettePostProcess;
class HorizontalBlurPostProcess;
class VerticalBlurPostProcess;
class SkyBox;
//test
class PointSpriteGSEffect;
class InstancedModel;
class BloomCombinePostProcess;
class PostProcessChain;
class BloomBlurPass;;
class GBufferPass;

class Renderer
{
public:

    Renderer() = default;
    ~Renderer();

    bool Initialize(Graphics* graphics);

    void BeginFrame(Camera* camera, float r, float g, float b, float a);
    void EndFrame();

    // 距離計算を含めてモデルを適切なパスに登録する
    void Submit(Model* model, RenderPass pass,BlendMode mode);
    void Execute();

    // ステート制御
    void SetCullMode(RasterizerStates::CullMode mode);
    void BeginStencilOutlinePass();
    void EndStencilOutlinePass();

    //defaultRTに戻してから描画するQuadオブジェクト
    bool createFinalRenderQuad();
    //ライト
    void UpdateLightDataConstantBuffer();
    void SubmitShadowPass();
    void UpdatePointLightConstantBuffer();

    //ポストプロセスバッファ
    void UpdatePostProcessConstantBuffer();
    void SetExposure(float exposure) { m_postProcessData.exposure = exposure; }

	//SSAO
        //ノイズテクスチャんの生成メソッド
    bool initSSAO(ID3D11Device* pDevice);
    bool initSSAOSampler(ID3D11Device* pDevice);
	    //cbufferの更新メソッド
    bool initSSAOConstantBuffer(ID3D11Device* pDevice);
    void updateSSAOConstantBuffer(ID3D11DeviceContext* pContext);

private:
    void UpdatePerFrameConstantBuffer();

private:
    Graphics* m_graphics = nullptr;
    Camera* m_currentCamera = nullptr; // パラメータ更新や距離計算用に保持

    // メンバを独自ポインタからComPtrや適切なクラスポインタで管理
    RasterizerStates* m_rasterStates = nullptr;
    DepthStencilStates* m_dsStates = nullptr;
    BlendStates* m_blendStates = nullptr;
    // レンダーパスの数だけ、個別のRenderQueueを持つ
    RenderQueue m_renderQueues[static_cast<int>(RenderPass::Count)];

    ComPtr<ID3D11Buffer> m_perFrameCB;

    //オフスクリーンレンダーターゲット
	RenderTarget* m_offscreenRT=nullptr;
    RenderTarget* m_offscreenRTwithMSAA = nullptr;//test,MSAA
    RenderTarget* m_tmpRT = nullptr;//ピンポン設計にするためにもう一枚
    //test:bloom対応のrt
	RenderTarget* m_brightRTwithMSAA = nullptr;
	RenderTarget* m_brightRT = nullptr;
    RenderTarget* m_blurPingRT = nullptr;//計算の都合でもう一個必要だった
   

    //ポストプロセス後に描画するQuadのmodel
    //Model* m_finalRenderQuad = nullptr;
    ScreenBlitMaterial* m_finalRenderMat = nullptr;
    Mesh* m_finalRenderMesh = nullptr;
    Shader* m_finalRenderShader = nullptr;
    //test:postProcessクラス
    ScreenBlitPostProcess* m_finalRenderScreenBlitPostProcess = nullptr;//simpleBlit
   
        //新規：手間のかかるblurエフェクト。
	BloomCombinePostProcess* m_finalRenderBloomCombinePostProcess = nullptr;
        //テスト；postprocessを担当するクラス
	PostProcessChain* m_postProcessChain = nullptr;
        //例外的なbloomBlurは専門パスとして別クラスに
	BloomBlurPass* m_bloomBlurPass = nullptr;

    //スカイボックスオブジェクト
    SkyBox* m_pSkyBox = nullptr;
    //pointSprite
    PointSpriteGSEffect* m_pPointSpriteGSEffect = nullptr;
    //instancedModel
    InstancedModel* m_pInstancedModel = nullptr;

    //Light
        //共用
    ComPtr<ID3D11Buffer> m_lightCB;//種類を問わず全てのライトをここに入れる。LitShaderのPSで使う
    //DirectionalLight
    std::vector<DirectionalLight>     m_directionalLights;
    //shadowMap
    std::vector<ShadowMap>     m_shadowMaps;
    Shader* m_pShadowShader = nullptr;
    // Renderer.hに追加、shadow用サンプラーは１つの共用でいい
    ComPtr<ID3D11SamplerState> m_shadowSampler;

    //test:PointLight
    std::vector<PointLight> m_pointLights;
    ComPtr<ID3D11Buffer>m_pointLightCB;
        //shadowMap
    std::vector<ShadowCubeMap> m_shadowCubeMaps;
    Shader* m_pShadowCubeShader = nullptr;
        //ジオメトリシェーダ
    ID3D11GeometryShader* m_pShadowCubeGS = nullptr;
        //サンプラー
    ComPtr<ID3D11SamplerState> m_shadowCubeSampler;

    //ポストプロセスバッファ、参照
    ComPtr<ID3D11Buffer> m_pPostProcessCB = nullptr;
    PostProcessConstantBuffer m_postProcessData;

    //test:遅延シェーダ:不透明オブジェクトは以下３つからなるg-bufferに必要情報を保存。
        //シェーダ
	Shader* m_pDeferredGBufferShader = nullptr;
	Shader* m_pDeferredLightingShader = nullptr;
        //サンプラー
    ComPtr<ID3D11SamplerState> m_gBufferDepthSampler; // ★追加


    //SSAO(アンビエントオカルージョン光)
        //rt
    RenderTarget* m_ssaoRawRT;
    RenderTarget* m_ssaoBlurRT;
        //ノイズテクスチャのsrv
	ComPtr<ID3D11ShaderResourceView> m_ssaoNoiseTextureSRV;
	    //半球サンプルの定数バッファ,c側データ
    ComPtr<ID3D11Buffer> m_ssaoCB = nullptr;
    SSAOParam     m_ssaoParamData;          // CPU側のデータ保持用
        //SSAO作成シェーダ
	Shader* m_pSSAOShader = nullptr;
	    //SSAOブラーシェーダ
	Shader* m_pSSAOBlurShader = nullptr;
        // SSAO関連で必要になる3つのサンプラー
    ID3D11SamplerState* m_pSamPointClamp = nullptr; // SSAO用: G-Buffer読込
    ID3D11SamplerState* m_pSamPointWrap = nullptr; // SSAO用: ノイズタイリング
    ID3D11SamplerState* m_pSamLinearClamp = nullptr; // ブラー用: 線形補間ぼかし

    //新規：Gbuffer
	GBufferPass* m_gBufferPass = nullptr;

};