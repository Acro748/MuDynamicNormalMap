cbuffer ConstBuffer : register(b0)
{
    uint width;
    uint height;
    uint widthStart;
    uint heightStart;

    uint mipLevel;
    uint radius;
    uint padding2;
    uint padding3;
}

Texture2D<float4> src        : register(t0);
RWTexture2D<float4> dst      : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    int2 coord = int2(threadID.xy) + int2(widthStart, heightStart);
    if (coord.x >= (int)width || coord.y >= (int)height)
        return;

    float4 orgPixel = src.Load(int3(coord, mipLevel));

    if (orgPixel.a == 1.0f)
    {
        dst[coord] = orgPixel;
        return;
    }

    float3 averageColor = float3(0.0f, 0.0f, 0.0f);
    int validCount = 0;

    for (int r = 1; r <= (int)radius; r++) {
        int x_start = -r;
        int y_start = -r;
        int x_end = r;
        int y_end = r;

        for (int x = x_start; x <= x_end; x++)
        {
            int2 nearCoord = coord + int2(x, y_start);
            if (nearCoord.x > 0 && nearCoord.y > 0 &&
                nearCoord.x < (int)width && nearCoord.y < (int)height)
            {
                float4 nearPixel = src.Load(int3(nearCoord, mipLevel));
                if (nearPixel.a == 1.0f)
                {
                    averageColor += nearPixel.rgb;
                    validCount++;
                }
            }

            nearCoord = coord + int2(x, y_end);
            if (nearCoord.x > 0 && nearCoord.y > 0 &&
                nearCoord.x < (int)width && nearCoord.y < (int)height)
            {
                float4 nearPixel = src.Load(int3(nearCoord, mipLevel));
                if (nearPixel.a == 1.0f)
                {
                    averageColor += nearPixel.rgb;
                    validCount++;
                }
            }
        }
        for (int y = y_start + 1; y < y_end; y++)
        {
            int2 nearCoord = coord + int2(x_start, y);
            if (nearCoord.x > 0 && nearCoord.y > 0 &&
                nearCoord.x < (int)width && nearCoord.y < (int)height)
            {
                float4 nearPixel = src.Load(int3(nearCoord, mipLevel));
                if (nearPixel.a == 1.0f)
                {
                    averageColor += nearPixel.rgb;
                    validCount++;
                }
            }

            nearCoord = coord + int2(x_end, y);
            if (nearCoord.x > 0 && nearCoord.y > 0 &&
                nearCoord.x < (int)width && nearCoord.y < (int)height)
            {
                float4 nearPixel = src.Load(int3(nearCoord, mipLevel));
                if (nearPixel.a == 1.0f)
                {
                    averageColor += nearPixel.rgb;
                    validCount++;
                }
            }
        }
        if (validCount > 0)
            break;
    }

    if (validCount == 0)
    {
        dst[coord] = orgPixel;
        return;
    }
    
    float4 resultColor = float4(averageColor / validCount, 1.0f);
    dst[coord] = resultColor;
}
