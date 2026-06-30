cbuffer ConstBuffer : register(b0)
{
    uint width;
    uint height;
    uint widthStart;
    uint heightStart;

    uint mipLevel;
    uint padding1;
    uint srcWidth;
    uint srcHeight;
}

Texture2D<float4> src       : register(t0);
RWTexture2D<float4> dst      : register(u0);

SamplerState samplerState : register(s0);

static const int2 offsets[8] = {
    int2(-1, -1), // left up
    int2(0, -1), // up
    int2(1, -1), // right up
    int2(-1,  0), // left
    int2(1,  0), // right
    int2(-1,  1), // left down
    int2(0,  1), // down
    int2(1,  1)  // right down
};

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    uint2 coord = uint2(threadID.xy) + uint2(widthStart, heightStart);
    if (any(coord >= uint2(width, height)))
        return;

    float4 resultColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float4 orgPixel = src.Load(uint3(coord, 0));
    if (orgPixel.a == 1.0f)
    {
        dst[coord] = orgPixel;
        return;
    }

    float3 averageColor = float3(0.0f, 0.0f, 0.0f);
    uint validCount = 0;

    [unroll]
    for (uint i = 0; i < 8; i++)
    {
        int2 nearCoord = int2(coord) + offsets[i];
        if (any(nearCoord < 0) || any(nearCoord >= int2(width, height)))
            continue;
        float4 nearPixel = src.Load(uint3(nearCoord, 0));
        if (nearPixel.a == 1.0f)
        {
            averageColor += nearPixel.rgb;
            validCount++;
        }
    }

    if (validCount == 0)
    {
        dst[coord] = orgPixel;
        return;
    }
    resultColor = float4(averageColor * rcp((float)validCount), 1.0f);

    dst[coord] = resultColor;
    return;
}
