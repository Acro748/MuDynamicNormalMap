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

static const uint2 sampleOffsets[4] = {
    int2(0, 0), // left up
    int2(1, 0), // right up
    int2(0, 1), // left down
    int2(1, 1)  // right down
};

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    uint2 coord = uint2(threadID.xy) + uint2(widthStart, heightStart);
    if (any(coord >= uint2(width, height)))
        return;

    float4 resultColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 averageColor = float3(0.0f, 0.0f, 0.0f);
    uint validCount = 0;

    [unroll]
    for (uint i = 0; i < 4; i++)
    {
        uint2 srcCoord = coord * 2 + sampleOffsets[i];
        if (any(srcCoord >= uint2(srcWidth, srcHeight)))
            continue;
        float4 srcPixel = src.Load(uint3(srcCoord, 0));
        if (srcPixel.a == 1.0f)
        {
            averageColor += srcPixel.rgb;
            validCount++;
        }
    }
    if (validCount == 0)
    {
        uint2 baseSrcCoord = coord * 2;

        [unroll]
        for (uint i = 0; i < 8; i++)
        {
            [unroll]
            for (uint j = 0; j < 4; j++)
            {
                int2 localOffset = offsets[i] * 2 + (int2)sampleOffsets[j];
                int2 srcCoord = (int2)baseSrcCoord + localOffset;
                if (any((uint2)srcCoord >= uint2(srcWidth, srcHeight)))
                    continue;
                float4 srcPixel = src.Load(uint3(srcCoord, 0));
                if (srcPixel.a == 1.0f)
                {
                    averageColor += srcPixel.rgb;
                    validCount++;
                }
            }
        }
    }
    if (validCount == 0)
    {
        dst[coord] = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }
    resultColor = float4(averageColor * rcp((float)validCount), 1.0f);

    dst[coord] = resultColor;
    return;
}
