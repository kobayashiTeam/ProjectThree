#pragma once
#include<d3d11.h>
#include<vector>
#include <wrl/client.h>

#include"shader.h"
#include"light.h"
#include"shadowMap.h"
#include"shadowCubeMap.h"
#include"cascadedShadowMap.h"
#include"renderQueue.h"
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

class Camera;

class ShadowSystem {
public:
    bool Initialize(ID3D11Device* pDevice);

    // ===== Directional（旧：20x20固定の1枚。フォワード系シェーダーはこちらを使い続ける） =====
    void BeginDirectionalPass(ID3D11DeviceContext* ctx);
    void EndDirectionalPass(ID3D11DeviceContext* ctx) {
        m_shadowMaps[0].EndRender(ctx);
    }

    // ===== Directional（CSM：Deferred専用の新経路） =====
    // 毎フレーム、カメラの視錐台から各段の行列を計算し、b7に転送する
    // CSMが無効なフレームでも「無効」というフラグをb7で伝える必要があるため、毎フレーム必ず呼ぶ
    void UpdateCascades(ID3D11DeviceContext* ctx, const Camera* camera);
    void BeginCascadePass(ID3D11DeviceContext* ctx, int cascadeIndex);
    void EndCascadePass(ID3D11DeviceContext* ctx) {
        m_cascadedShadowMap.EndRender(ctx);
    }

    // ===== Point =====
    void BeginPointPass(ID3D11DeviceContext* ctx) {
        m_shadowCubeMaps[0].BeginRender(ctx);
        m_shadowCubeShader->Bind(ctx);
        ctx->GSSetShader(m_shadowCubeGS, nullptr, 0);
    }
    void EndPointPass(ID3D11DeviceContext* ctx) {
        m_shadowCubeMaps[0].EndRender(ctx);
        ctx->GSSetShader(nullptr, nullptr, 0);
    }

    // ===== Lightingパスで使うためのバインド =====
    void BindForLighting(ID3D11DeviceContext* ctx) {
        auto* srv = m_shadowMaps[0].GetSRV();
        ctx->PSSetShaderResources(3, 1, &srv);
        ID3D11SamplerState* sampler = m_shadowSampler.Get();
        ctx->PSSetSamplers(1, 1, &sampler);

        srv = m_shadowCubeMaps[0].GetSRV();
        ctx->PSSetShaderResources(4, 1, &srv);
        sampler = m_shadowCubeSampler.Get();
        ctx->PSSetSamplers(2, 1, &sampler);

        // CSM：配列テクスチャはt15、行列と境界はb7（サンプラーは旧シャドウと同じ比較サンプラーs1を共用）
        srv = m_cascadedShadowMap.GetSRV();
        ctx->PSSetShaderResources(15, 1, &srv);
        ID3D11Buffer* cb = m_cascadeCB.Get();
        ctx->PSSetConstantBuffers(7, 1, &cb);
    }

    void UpdatePointLightConstantBuffer(ID3D11DeviceContext* ctx);
    void UpdateLightDataConstantBuffer(ID3D11DeviceContext* ctx);
    void SubmitShadowPass(RenderQueue& opaqueQueue) {
        opaqueQueue.SetOverrideVS(m_shadowShader);
    }
    // DirectionalLightの方向を更新する
    void SetDirectionalLightDirection(DirectX::XMFLOAT3 dir) {
        if (!m_directionalLights.empty()) {
            m_directionalLights[0].direction = dir;
        }
    }
    // PointLightの位置を更新する
    void SetPointLightPosition(DirectX::XMFLOAT3 pos) {
        if (!m_pointLights.empty()) {
            m_pointLights[0].position = pos;
        }
    }
    // PointLightの強さを更新する
    void SetPointLightIntensity(float intensity) {
        if (!m_pointLights.empty()) {
            m_pointLights[0].intensity = intensity;
        }
    }

    // 有効化するライト種別を設定する
    void SetLightVisibilityMode(LightVisibilityMode mode) {
        m_lightVisibilityMode = mode;
    }

    // ===== このフレームでシャドウマップを描く必要があるか =====
    // シェーダーがマップを読むかどうかは「b3のlights[]に詰めたライトの種類」で決まり、
    // lights[]はモードを見て詰めている（UpdateLightDataConstantBuffer）。
    // なので「描くかどうか」も同じモードから決め、「読むときは必ず描いてある」を保つ。
    // （描かなかったマップには古い深度が残るが、読まれないので見た目には出ない）
    bool IsDirectionalLit() const {
        return m_lightVisibilityMode == LightVisibilityMode::DirectionalOnly ||
            m_lightVisibilityMode == LightVisibilityMode::Both;
    }
    bool IsPointLit() const {
        return m_lightVisibilityMode == LightVisibilityMode::PointOnly ||
            m_lightVisibilityMode == LightVisibilityMode::Both;
    }
    // 旧方向光マップ(t3)：Deferredでも CSM無効なら読む。
    // フォワード系(LitShaderなど)は CSMを知らずに常にt3を読むので、フォワードの物体がいれば必要
    bool NeedsDirectionalShadow(bool hasForwardObjects) const {
        return IsDirectionalLit() && (!m_cascadeEnabled || hasForwardObjects);
    }
    bool NeedsCascadeShadow() const {
        return IsDirectionalLit() && m_cascadeEnabled;
    }
    bool NeedsPointShadow() const {
        return IsPointLit();
    }

    // ===== CSMの設定 =====
    void SetCascadeEnabled(bool isOn) { m_cascadeEnabled = isOn; }
    bool IsCascadeEnabled() const { return m_cascadeEnabled; }
    void SetCascadeDebug(bool isOn) { m_cascadeDebug = isOn; }
    // λ：1.0で純粋な対数分割、0.0で純粋な等間隔分割
    void SetCascadeLambda(float lambda) { m_cascadeLambda = lambda; }
    float GetCascadeLambda() const { return m_cascadeLambda; }
    // 各段の奥側の境界（UI表示用）
    float GetCascadeSplit(int i) const { return m_cascadeSplits[i]; }

private:
    // シャドウパスVS用のb8に、今描いている1枚の行列を書き込む
    void UploadShadowPassMatrix(ID3D11DeviceContext* ctx, DirectX::FXMMATRIX lightViewProj);

private:
    std::vector<DirectionalLight> m_directionalLights;
    std::vector<PointLight> m_pointLights;
    LightVisibilityMode m_lightVisibilityMode = LightVisibilityMode::Both;
    std::vector<ShadowMap> m_shadowMaps;
    std::vector<ShadowCubeMap> m_shadowCubeMaps;

    Shader* m_shadowShader = nullptr;
    Shader* m_shadowCubeShader = nullptr;
    ID3D11GeometryShader* m_shadowCubeGS = nullptr;

    ComPtr<ID3D11SamplerState> m_shadowSampler;
    ComPtr<ID3D11SamplerState> m_shadowCubeSampler;
    // ShadowCubeMap用シェーダーの定数バッファ
    ComPtr<ID3D11Buffer> m_pointLightCB;
    ComPtr<ID3D11Buffer> m_lightCB;
    // シャドウパスVS用（b8）
    ComPtr<ID3D11Buffer> m_shadowPassCB;

    // ===== CSM =====
    CascadedShadowMap    m_cascadedShadowMap;
    ComPtr<ID3D11Buffer> m_cascadeCB;                   // b7
    DirectX::XMFLOAT4X4  m_cascadeViewProj[NUM_CASCADES] = {}; // シャドウパス描画時にb8へ渡す用（転置前）
    float m_cascadeSplits[NUM_CASCADES] = {};

    bool  m_cascadeEnabled = false;
    bool  m_cascadeDebug = false;
    float m_cascadeLambda = 0.75f;   // 分割の混ぜ具合
    float m_shadowDistance = 60.0f;  // 影を出す最大距離（カメラのfar=1000とは別に割り切る）
    float m_splitNear = 1.0f;        // 対数分割の計算に使うnear。カメラのnear(0.01)をそのまま使うと1段目が極端に薄くなる
    float m_casterExtension = 30.0f; // 視錐台の外（光源側）にある影を落とす物体を拾うため、正射影の奥行きを手前に延ばす量
    float m_baseBiasWorld = 0.05f;   // 深度バイアスの基本量（ワールド単位、m）
};
