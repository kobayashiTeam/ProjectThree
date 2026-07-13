#include"shadowSystem.h"
#include"shaderManager.h"

bool ShadowSystem::Initialize(ID3D11Device* pDevice) {

    // ライトCB生成（旧: 201行目）
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(LightBufferCB);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    pDevice->CreateBuffer(&bd, nullptr, &m_lightCB);

    // Directionalライト生成＋シャドウマップ（旧: 205-236行目）
    DirectionalLight dirLight = {};
    dirLight.type = LightType::Directional;
    dirLight.position = { -3.0f, 5.0f, -10.0f };
    dirLight.direction = { 3.0f, -1.0f, 1.0f };
    dirLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    dirLight.intensity = 0.80f;
    m_directionalLights.reserve(MAX_LIGHTS);   // ★ポインタ安全性のため必須
    m_directionalLights.push_back(dirLight);

    ShadowMap shadowMap;
    shadowMap.Initialize(pDevice, 2048);
    shadowMap.setLight(&m_directionalLights[0]);
    m_shadowMaps.reserve(MAX_LIGHTS);
    m_shadowMaps.push_back(shadowMap);

    m_shadowShader = ShaderManager::GetInstance().GetShader(ShaderID::Shadow);
    // シャドウサンプラー生成（旧: 224-236行目）...
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

    // Pointライト生成＋シャドウキューブマップ（旧: 239-274行目）
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
    shadowCubeMap.SetLight(&m_pointLights[0]);  // ※元コードはローカル変数pointLightのアドレスを渡していたが、
    //   これは危険（関数を抜けると無効ポインタ）なので、
    //   vector内の実体を指すよう修正しておきます
    m_shadowCubeMaps.reserve(MAX_LIGHTS);
    m_shadowCubeMaps.push_back(shadowCubeMap);

    m_shadowCubeShader = ShaderManager::GetInstance().GetShader(ShaderID::ShadowCube);
    m_shadowCubeGS = ShaderManager::GetInstance().getGS(ShaderID::ShadowCubeGS);
    // 通常サンプラー生成（旧: 265-274行目）...
    sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    pDevice->CreateSamplerState(&sampDesc, &m_shadowCubeSampler);

    return true;
}


void ShadowSystem:: UpdateLightDataConstantBuffer(ID3D11DeviceContext* ctx) {

    LightBufferCB cb = {};

    // Directional
    for (int i = 0; i < (int)m_directionalLights.size() && cb.lightCount < MAX_LIGHTS; i++)
    {
        const DirectionalLight& L = m_directionalLights[i];
        auto& dst = cb.lights[cb.lightCount];
        dst.position = { L.position.x, L.position.y, L.position.z, 0.0f };
        dst.direction = { L.direction.x, L.direction.y, L.direction.z, 0.0f };
        dst.color = L.color;
        dst.intensity = L.intensity;
        dst.type = (int)LightType::Directional;
        dst.farPlane = 0.0f;  // 未使用
        DirectX::XMMATRIX lsm = L.GetViewMatrix() * L.GetProjectionMatrix();
        dst.lightSpaceMatrix = DirectX::XMMatrixTranspose(lsm);
        cb.lightCount++;
    }

    // Pointを続けて詰める
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

    ctx->UpdateSubresource(m_lightCB.Get(), 0, nullptr, &cb, 0, 0);

    ID3D11Buffer* cbArray[] = { m_lightCB.Get() };
    ctx->VSSetConstantBuffers(3, 1, cbArray);
    ctx->PSSetConstantBuffers(3, 1, cbArray);
}


void ShadowSystem::UpdatePointLightConstantBuffer(ID3D11DeviceContext* ctx) {

    if (m_pointLights.empty()) return;

    ShadowCubeCB cb = {};
    const PointLight& L = m_pointLights[0];  // 今は1灯固定

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
    ctx->GSSetConstantBuffers(4, 1, cbArray);  // GSにも忘れず
    ctx->PSSetConstantBuffers(4, 1, cbArray);
}