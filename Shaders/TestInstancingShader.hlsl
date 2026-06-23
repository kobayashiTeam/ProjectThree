// Lit_InstancingShader.hlsl

cbuffer PerFrameBuffer : register(b0)
{
    matrix mView;
    matrix mProjection;
    float4 vLightPos;
    float4 vLightColor;
    float4 vEyePos;
    float4 vAttenuation;
};

cbuffer PerMaterialBuffer : register(b2)
{
    float4 vMaterialColor;
};

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;

    float4 InstMatrixRow0 : INSTANCE_WORLD0;
    float4 InstMatrixRow1 : INSTANCE_WORLD1;
    float4 InstMatrixRow2 : INSTANCE_WORLD2;
    float4 InstMatrixRow3 : INSTANCE_WORLD3;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float2 Tex : TEXCOORD0;
    float3 Normal : NORMAL;
    float3 WorldPos : TEXCOORD1;
};

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT) 0;

    float4x4 mWorld = float4x4(
        input.InstMatrixRow0,
        input.InstMatrixRow1,
        input.InstMatrixRow2,
        input.InstMatrixRow3
    );

    float4 worldPos = mul(input.Pos, mWorld);
    float4 viewPos = mul(worldPos, mView);
    output.Pos = mul(viewPos, mProjection);

    output.Normal = normalize(mul(float4(input.Normal, 0.0f), mWorld).xyz);
    output.WorldPos = worldPos.xyz;
    output.Color = input.Color;
    output.Tex = input.Tex;

    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    float4 texColor = txDiffuse.Sample(samLinear, input.Tex);
    float4 objectColor = texColor * input.Color * vMaterialColor;

    float3 lightVec = vLightPos.xyz - input.WorldPos;
    float dist = length(lightVec);
    float3 lightDir = normalize(lightVec);

    float atten = 1.0f / (vAttenuation.x
                        + vAttenuation.y * dist
                        + vAttenuation.z * dist * dist);

    float3 ambient = 0.2f * vLightColor.xyz;

    float3 N = normalize(input.Normal);
    float diff = max(dot(N, lightDir), 0.0f);
    float3 diffuse = diff * vLightColor.xyz;

    float3 viewDir = normalize(vEyePos.xyz - input.WorldPos);
    float3 halfway = normalize(lightDir + viewDir);
    float spec = pow(max(dot(N, halfway), 0.0f), 32.0f);
    float3 specular = 0.5f * spec * vLightColor.xyz;

    ambient *= atten;
    diffuse *= atten;
    specular *= atten;

    float3 final = (ambient + diffuse) * objectColor.xyz + specular;
    return float4(final, objectColor.a);
}