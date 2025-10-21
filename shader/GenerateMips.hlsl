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

Texture2D<float4> src0       : register(t0);
RWTexture2D<float4> dst      : register(u0);
RWTexture2D<float4> src      : register(u1);

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
    if (coord.x >= width || coord.y >= height)
        return;

	float4 resultColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (mipLevel == 0)
    {
        float4 orgPixel = src0.Load(uint3(coord, mipLevel));
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
            if (nearCoord.x < 0 || nearCoord.y < 0 ||
                nearCoord.x >= (int)(width) || nearCoord.y >= (int)(height))
                continue;
            float4 nearPixel = src0.Load(uint3(uint2(nearCoord), mipLevel));
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
        resultColor = float4(averageColor / validCount, 1.0f);
	}
    else
    {
        float3 averageColor = float3(0.0f, 0.0f, 0.0f);
        uint validCount = 0;

        [unroll]
        for (uint i = 0; i < 4; i++)
        {
            uint2 srcCoord = coord * 2 + sampleOffsets[i];
            if (srcCoord.x >= srcWidth || srcCoord.y >= srcHeight)
				continue;
            float4 srcPixel = src[srcCoord];
            if (srcPixel.a == 1.0f)
            {
                averageColor += srcPixel.rgb;
                validCount++;
            }
		}
        if (validCount == 0)
        {
            [unroll]
            for (uint i = 0; i < 8; i++)
            {
				int2 nearCoord = int2(coord) + offsets[i];
                if (nearCoord.x < 0 || nearCoord.y < 0 ||
                    nearCoord.x >= (int)(width) || nearCoord.y >= (int)(height))
					continue;

                [unroll]
                for (uint i = 0; i < 4; i++)
                {
                    uint2 srcCoord = uint2(nearCoord) * 2 + sampleOffsets[i];
                    if (srcCoord.x >= srcWidth || srcCoord.y >= srcHeight)
                        continue;
                    float4 srcPixel = src[srcCoord];
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
        resultColor = float4(averageColor / validCount, 1.0f);
    }

    dst[coord] = resultColor;
    return;
}
