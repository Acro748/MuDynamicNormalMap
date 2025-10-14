cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint widthStart;
    uint heightStart;
}

Texture2D<float4> srcTexture1 : register(t0);
Texture2D<float4> srcTexture2 : register(t1);
RWTexture2D<float4> dstTexture : register(u0);

SamplerState samplerState : register(s0);

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    int2 coord = int2(threadID.xy) + int2(widthStart, heightStart);
    if (coord.x >= (int)texWidth || coord.y >= (int)texHeight)
        return;

    float2 uv = (float2(coord) + 0.5f) / float2(texWidth, texHeight);
    float4 srcColor1 = srcTexture1.SampleLevel(samplerState, uv, 0);
    float4 srcColor2 = srcTexture2.SampleLevel(samplerState, uv, 0);
    if (srcColor1.a == 0.0f && srcColor2.a == 0.0f)
        return;

    float4 dstColor = float4(lerp(srcColor1.rgb, srcColor2.rgb, srcColor2.a), 1.0f);
    dstTexture[coord] = dstColor;
}
