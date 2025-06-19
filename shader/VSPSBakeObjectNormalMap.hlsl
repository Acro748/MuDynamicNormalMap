cbuffer Transform : register(b0)
{
    float4x4 modelMatrix;
    float4x4 viewProjectionMatrix; // Ãß°¡
};

Texture2D originalTex : register(t0);
Texture2D maskTex : register(t1);
SamplerState samplerState : register(s0);

struct VS_INPUT
{
    float3 pos : POSITION;
    float3 norm : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float3 norm : TEXCOORD1;
    float2 uv : TEXCOORD0;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(viewProjectionMatrix, mul(modelMatrix, float4(input.pos, 1.0f)));
    output.norm = input.norm;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);

    float3 normal = 0.5f * (normalize(input.norm) + 1.0f);

    float4 origColor = originalTex.Sample(samplerState, input.uv);
    float4 maskColor = maskTex.Sample(samplerState, input.uv);

    float blendAlpha = saturate(maskColor.a);
    float4 finalColor = lerp(origColor, float4(normal, 1.0f), blendAlpha);

    return finalColor;
}
