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
class SkyBox;
//test
class PointSpriteGSEffect;
class InstancedModel;


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
    // ★【核心】このフレームで「実行する予定の全エフェクト」を並べるコンテナ
    std::vector<PostProcess*> m_postProcessChain;


    //ポストプロセス後に描画するQuadのmodel
    //Model* m_finalRenderQuad = nullptr;
    ScreenBlitMaterial* m_finalRenderMat = nullptr;
    Mesh* m_finalRenderMesh = nullptr;
    Shader* m_finalRenderShader = nullptr;
    //test:postProcessクラス
    ScreenBlitPostProcess* m_finalRenderScreenBlitPostProcess = nullptr;//simpleBlit
    MonochromePostProcess* m_finalRenderMonochromePostProcess = nullptr;//monochrome
    InversionPostProcess* m_finalRenderInversionPostProcess = nullptr;//inversion
    SepiaPostProcess* m_finalRenderSepiaPostProcess = nullptr;
    SimpleBoxBlurPostProcess* m_finalRenderSimpleBoxBluer = nullptr;
    SharpenPostProcess* m_finalRenderSharpenPostProcess = nullptr;
    VignettePostProcess* m_finalRenderVignettePostProcess = nullptr;

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

};