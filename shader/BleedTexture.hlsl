cbuffer Params : register(b0)
{
    uint width;
    uint height;
    uint padding1;
    uint mipLevel;
}

Texture2D<float4> src        : register(t0);
RWTexture2D<float4> dst      : register(u0);

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
    int2 coord = int2(threadID.xy);
    if (coord.x >= (int)width || coord.y >= (int)height)
        return;

    float4 orgPixel = src.Load(int3(coord, mipLevel));

    if (orgPixel.a > 0.5f)
    {
        return;
    }

    float4 averageColor = float4(0, 0, 0, 0);
    int validCount = 0;

    for (uint i = 0; i < 8; i++)
    {
        int2 nearCoord = coord + offsets[i];

        if (nearCoord.x < 0 || nearCoord.y < 0 ||
            nearCoord.x >= (int)width || nearCoord.y >= (int)height)
            continue;

        float4 nearPixel = src.Load(int3(nearCoord, mipLevel));

        if (nearPixel.a > 0.5f)
        {
            averageColor += nearPixel;
            validCount++;
        }
    }

    if (validCount == 0)
        return;
    
    float4 resultColor = averageColor / validCount;
    resultColor.a = 1.0f;
    dst[coord] = resultColor;
}
