#include"shadowSystem.h"
#include"shaderManager.h"
#include"camera.h"
#include<cmath>

bool ShadowSystem::Initialize(ID3D11Device* pDevice) {

    // ライトCB生成
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(LightBufferCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pDevice->CreateBuffer(&bd, nullptr, &m_lightCB);

    // Directionalライト生成＋シャドウマップ
    DirectionalLight dirLight = {};
    dirLight.type = LightType::Directional;
    dirLight.position = { -3.0f, 5.0f, -10.0f };
    dirLight.direction = { 3.0f, -1.0f, 1.0f };
    dirLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    dirLight.intensity = 0.80f;
    m_directionalLights.reserve(MAX_LIGHTS);   //ポインタ安全性のため必須
    m_directionalLights.push_back(dirLight);

    ShadowMap shadowMap;
    shadowMap.Initialize(pDevice, 2048);
    shadowMap.setLight(&m_directionalLights[0]);
    m_shadowMaps.reserve(MAX_LIGHTS);
    m_shadowMaps.push_back(shadowMap);

    m_shadowShader = ShaderManager::GetInstance().GetShader(ShaderID::Shadow);
    // シャドウサンプラー生成
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sampDesc.BorderColor[0] = 1.0f; // 範囲外は影なし
    sampDesc.BorderColor[1] = 1.0f;
    sampDesc.BorderColor[2] = 1.0f;
    sampDesc.BorderColor[3] = 1.0f;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

    pDevice->CreateSamplerState(&sampDesc, &m_shadowSampler);

    // Pointライト生成＋シャドウキューブマップ
    bd = {};
    bd.ByteWidth = sizeof(ShadowCubeCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pDevice->CreateBuffer(&bd, nullptr, &m_pointLightCB);

    PointLight pointLight = {};
    pointLight.position = { 5.0f, 5.0f, 3.0f };
    pointLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLight.intensity = 1.0f;
    m_pointLights.reserve(MAX_LIGHTS);
    m_pointLights.push_back(pointLight);

    ShadowCubeMap shadowCubeMap;
    shadowCubeMap.Initialize(pDevice, 2048);
    shadowCubeMap.SetLight(&m_pointLights[0]);
    m_shadowCubeMaps.reserve(MAX_LIGHTS);
    m_shadowCubeMaps.push_back(shadowCubeMap);

    m_shadowCubeShader = ShaderManager::GetInstance().GetShader(ShaderID::ShadowCube);
    m_shadowCubeGS = ShaderManager::GetInstance().getGS(ShaderID::ShadowCubeGS);
    // 通常サンプラー生成
    sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    pDevice->CreateSamplerState(&sampDesc, &m_shadowCubeSampler);

    // シャドウパスVS用CB（b8）
    bd = {};
    bd.ByteWidth = sizeof(ShadowPassCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &m_shadowPassCB))) return false;

    // CSM：配列テクスチャとb7用CB
    if (!m_cascadedShadowMap.Initialize(pDevice, CASCADE_SHADOW_MAP_SIZE)) return false;

    bd = {};
    bd.ByteWidth = sizeof(CascadeShadowCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &m_cascadeCB))) return false;

    return true;
}


void ShadowSystem::UploadShadowPassMatrix(ID3D11DeviceContext* ctx, DirectX::FXMMATRIX lightViewProj)
{
    ShadowPassCB cb = {};
    cb.lightViewProj = DirectX::XMMatrixTranspose(lightViewProj);
    ctx->UpdateSubresource(m_shadowPassCB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbArray[] = { m_shadowPassCB.Get() };
    ctx->VSSetConstantBuffers(8, 1, cbArray);
}


void ShadowSystem::BeginDirectionalPass(ID3D11DeviceContext* ctx)
{
    m_shadowMaps[0].BeginRender(ctx);
    m_shadowShader->Bind(ctx);

    // 旧：ShadowShaderがb3のlights[0]を読んでいた → 方向光の行列を直接b8に渡す形に変更
    // （PointOnlyモードでlights[0]が点光源になっても、正しい行列で描画される）
    if (!m_directionalLights.empty()) {
        const DirectionalLight& L = m_directionalLights[0];
        UploadShadowPassMatrix(ctx, L.GetViewMatrix() * L.GetProjectionMatrix());
    }
}


void ShadowSystem::BeginCascadePass(ID3D11DeviceContext* ctx, int cascadeIndex)
{
    m_cascadedShadowMap.BeginRender(ctx, cascadeIndex);
    m_shadowShader->Bind(ctx);  // シェーダーは旧シャドウパスと同じ。違うのはb8に入る行列だけ
    UploadShadowPassMatrix(ctx, DirectX::XMLoadFloat4x4(&m_cascadeViewProj[cascadeIndex]));
}


// =========================================================
// CSMの中心：カメラの視錐台を分割し、段ごとに「方眼紙をどこにどれだけ敷くか」を決める
// =========================================================
void ShadowSystem::UpdateCascades(ID3D11DeviceContext* ctx, const Camera* camera)
{
    using namespace DirectX;

    CascadeShadowCB cb = {};
    cb.cascadeEnabled = m_cascadeEnabled ? 1 : 0;
    cb.cascadeDebug = m_cascadeDebug ? 1 : 0;
    cb.cascadeTexelSize = 1.0f / (float)CASCADE_SHADOW_MAP_SIZE;

    if (m_cascadeEnabled && camera && !m_directionalLights.empty())
    {
        XMMATRIX view = camera->GetViewMatrix();
        XMMATRIX proj = camera->GetProjectionMatrix();

        // ---------------------------------------------------------
        // (1) カメラのnear/farを射影行列から逆算する
        //     Cameraクラスはnear/farを保持していないが、XMMatrixPerspectiveFovLHの中身は
        //       _33 = f/(f-n),  _43 = -n*f/(f-n)
        //     なので、ここから n = -_43/_33,  f = _43/(1-_33) と戻せる
        // ---------------------------------------------------------
        XMFLOAT4X4 p;
        XMStoreFloat4x4(&p, proj);
        float camNear = -p._43 / p._33;
        float camFar = p._43 / (1.0f - p._33);
        float shadowFar = (m_shadowDistance < camFar) ? m_shadowDistance : camFar;

        // ---------------------------------------------------------
        // (2) 分割位置を決める（対数分割と等間隔分割をλで混ぜる：Practical Split Scheme）
        // ---------------------------------------------------------
        float splits[NUM_CASCADES + 1];
        splits[0] = camNear;
        for (int i = 1; i < NUM_CASCADES; i++)
        {
            float t = (float)i / (float)NUM_CASCADES;
            float logSplit = m_splitNear * powf(shadowFar / m_splitNear, t);
            float uniformSplit = camNear + (shadowFar - camNear) * t;
            splits[i] = m_cascadeLambda * logSplit + (1.0f - m_cascadeLambda) * uniformSplit;
        }
        splits[NUM_CASCADES] = shadowFar;

        // ---------------------------------------------------------
        // (3) 視錐台のnear面・far面の4隅をワールド空間で求める
        //     NDCの箱（x,y=±1, z=0/1）を (view*proj)の逆行列 で世界に戻す
        // ---------------------------------------------------------
        XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);
        const float ndcXY[4][2] = { {-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f} };
        XMVECTOR nearCorners[4], farCorners[4];
        for (int k = 0; k < 4; k++)
        {
            nearCorners[k] = XMVector3TransformCoord(XMVectorSet(ndcXY[k][0], ndcXY[k][1], 0.0f, 1.0f), invViewProj);
            farCorners[k] = XMVector3TransformCoord(XMVectorSet(ndcXY[k][0], ndcXY[k][1], 1.0f, 1.0f), invViewProj);
        }

        // ---------------------------------------------------------
        // (4) ライトの「向きだけ」のView行列（原点に置く）
        //     位置を含めないことで、方眼紙の向きと目盛りが世界に対して固定される
        //     → 後段のテクセルスナップが意味を持つ
        // ---------------------------------------------------------
        XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&m_directionalLights[0].direction));
        float dotY = fabsf(XMVectorGetY(lightDir));
        XMVECTOR up = (dotY > 0.99f) ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMMATRIX lightView = XMMatrixLookToLH(XMVectorZero(), lightDir, up);

        for (int c = 0; c < NUM_CASCADES; c++)
        {
            // (5) この段の8頂点：near隅→far隅の線上を、深度の比率で線形補間して切り出す
            //     （同じ視線の上ではビュー空間の深度は線形に変わるため、比率で補間できる）
            float t0 = (splits[c] - camNear) / (camFar - camNear);
            float t1 = (splits[c + 1] - camNear) / (camFar - camNear);
            XMVECTOR corners[8];
            for (int k = 0; k < 4; k++)
            {
                corners[k] = XMVectorLerp(nearCorners[k], farCorners[k], t0);
                corners[k + 4] = XMVectorLerp(nearCorners[k], farCorners[k], t1);
            }

            // (6) 8頂点を囲む「球」を作る
            //     AABBだとカメラの回転で大きさが変わり、方眼紙のマス目が伸び縮みしてちらつく
            XMVECTOR center = XMVectorZero();
            for (int k = 0; k < 8; k++) center = XMVectorAdd(center, corners[k]);
            center = XMVectorScale(center, 1.0f / 8.0f);

            float radius = 0.0f;
            for (int k = 0; k < 8; k++)
            {
                float d = XMVectorGetX(XMVector3Length(XMVectorSubtract(corners[k], center)));
                if (d > radius) radius = d;
            }
            // 浮動小数点の誤差で毎フレーム微妙に半径が揺れないよう、1/16単位で切り上げて固定する
            radius = ceilf(radius * 16.0f) / 16.0f;

            // (7) テクセルスナップ：球の中心を、ライト空間で「1テクセルの大きさ」の倍数に丸める
            //     → 方眼紙が世界の上を滑らず、1マス単位でカクッとしか動かなくなる
            float texelWorld = (2.0f * radius) / (float)CASCADE_SHADOW_MAP_SIZE;
            XMFLOAT3 cLS;
            XMStoreFloat3(&cLS, XMVector3TransformCoord(center, lightView));
            cLS.x = floorf(cLS.x / texelWorld) * texelWorld;
            cLS.y = floorf(cLS.y / texelWorld) * texelWorld;

            // (8) 正射影：xyは球の直径ぶん、zは光源側へcasterExtensionだけ余分に延ばす
            //     （画面外にあるが、画面内に影を落とす物体を取りこぼさないため）
            float zNear = cLS.z - radius - m_casterExtension;
            float zFar = cLS.z + radius;
            XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(
                cLS.x - radius, cLS.x + radius,
                cLS.y - radius, cLS.y + radius,
                zNear, zFar);

            XMMATRIX lightViewProj = lightView * lightProj;
            XMStoreFloat4x4(&m_cascadeViewProj[c], lightViewProj);
            cb.cascadeViewProj[c] = XMMatrixTranspose(lightViewProj);

            // (9) 深度バイアス：シャドウマップの深度は0〜1に正規化されているので、
            //     「ワールドで何mずらしたいか」を奥行きの幅で割って変換する。
            //     1テクセルが大きい奥の段ほど、面の傾きによる誤差（シャドウアクネ）も大きくなるため、テクセルの大きさに比例して増やす
            float depthRange = zFar - zNear;
            float bias = (m_baseBiasWorld + texelWorld * 1.5f) / depthRange;
            if (c == 0) cb.cascadeBias.x = bias;
            if (c == 1) cb.cascadeBias.y = bias;
            if (c == 2) cb.cascadeBias.z = bias;

            m_cascadeSplits[c] = splits[c + 1];
        }

        cb.cascadeSplits = XMFLOAT4(splits[1], splits[2], splits[3], 0.0f);
    }

    ctx->UpdateSubresource(m_cascadeCB.Get(), 0, nullptr, &cb, 0, 0);
    ID3D11Buffer* cbArray[] = { m_cascadeCB.Get() };
    ctx->PSSetConstantBuffers(7, 1, cbArray);
}


void ShadowSystem::UpdateLightDataConstantBuffer(ID3D11DeviceContext* ctx) {

    LightBufferCB cb = {};

    // Directional（DirectionalOnly / Both のときだけ詰める）
    if (m_lightVisibilityMode == LightVisibilityMode::DirectionalOnly ||
        m_lightVisibilityMode == LightVisibilityMode::Both)
    {
        for (int i = 0; i < (int)m_directionalLights.size() && cb.lightCount < MAX_LIGHTS; i++)
        {
            const DirectionalLight& L = m_directionalLights[i];
            auto& dst = cb.lights[cb.lightCount];
            dst.position = { L.position.x, L.position.y, L.position.z, 0.0f };
            dst.direction = { L.direction.x, L.direction.y, L.direction.z, 0.0f };
            dst.color = L.color;
            dst.intensity = L.intensity;
            dst.type = (int)LightType::Directional;
            dst.farPlane = 0.0f;
            DirectX::XMMATRIX lsm = L.GetViewMatrix() * L.GetProjectionMatrix();
            dst.lightSpaceMatrix = DirectX::XMMatrixTranspose(lsm);
            cb.lightCount++;
        }
    }

    // Point（PointOnly / Both のときだけ詰める）
    if (m_lightVisibilityMode == LightVisibilityMode::PointOnly ||
        m_lightVisibilityMode == LightVisibilityMode::Both)
    {
        for (int i = 0; i < (int)m_pointLights.size() && cb.lightCount < MAX_LIGHTS; i++)
        {
            const PointLight& L = m_pointLights[i];
            auto& dst = cb.lights[cb.lightCount];
            dst.position = { L.position.x, L.position.y, L.position.z, 0.0f };
            dst.color = L.color;
            dst.intensity = L.intensity;
            dst.type = (int)LightType::Point;
            dst.farPlane = L.farPlane;
            // lightSpaceMatrixは未使用なのでゼロのまま
            cb.lightCount++;
        }
    }

    ctx->UpdateSubresource(m_lightCB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbArray[] = { m_lightCB.Get() };
    ctx->VSSetConstantBuffers(3, 1, cbArray);
    ctx->PSSetConstantBuffers(3, 1, cbArray);
}


void ShadowSystem::UpdatePointLightConstantBuffer(ID3D11DeviceContext* ctx) {

    if (m_pointLights.empty()) return;

    ShadowCubeCB cb = {};
    const PointLight& L = m_pointLights[0];  // 現在は1灯のみ対応

    cb.gLightPos = L.position;
    cb.gFarPlane = L.farPlane;

    // 6面分のViewProj行列
    DirectX::XMMATRIX proj = L.GetProjectionMatrix();
    for (int i = 0; i < 6; i++)
    {
        DirectX::XMMATRIX vp = L.GetViewMatrix(i) * proj;
        cb.gLightViewProj[i] = DirectX::XMMatrixTranspose(vp);
    }

    ctx->UpdateSubresource(m_pointLightCB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbArray[] = { m_pointLightCB.Get() };
    ctx->VSSetConstantBuffers(4, 1, cbArray);
    ctx->GSSetConstantBuffers(4, 1, cbArray);
    ctx->PSSetConstantBuffers(4, 1, cbArray);
}