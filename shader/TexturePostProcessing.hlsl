cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint widthStart;
    uint heightStart;

    uint blurRadiusX;
    uint blurRadiusY;
    uint padding1;
    uint padding2;
}

Texture2D<float4> srcTexture : register(t0);
Texture2D<float4> maskTexture : register(t1);

RWTexture2D<float4> dstTexture : register(u0);

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
    int2 coord = int2(threadID.xy) + int2(widthStart, heightStart);
    if (coord.x >= texWidth || coord.y >= texHeight)
        return; 

	float2 uv = (float2(coord) + 0.5f) / float2(texWidth, texHeight);

    float4 src = srcTexture.SampleLevel(samplerState, uv, 0);
    float4 mask = maskTexture.SampleLevel(samplerState, uv, 0);
    if (mask.r == 0.0f)
    {
        dstTexture[coord] = src;
        return;
    }

    if (src.a < 1.0f)
        return;

    float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f;
    float weightSumR = 0.0f, weightSumG = 0.0f, weightSumB = 0.0f;

    for (uint by = -blurRadiusY; by <= blurRadiusY; by++)
    {
        if (by == 0)
            continue;
        for (uint bx = -blurRadiusX; bx <= blurRadiusX; bx++)
        {
            if (bx == 0)
                continue;
            int2 nearCoord = coord + int2(bx, by);
            if (nearCoord.x < 0 || nearCoord.y < 0 ||
                nearCoord.x >= (int)texWidth || nearCoord.y >= (int)texHeight)
                continue;

            float2 offsetUV = float2(nearCoord) / float2(texWidth, texHeight);
            float4 sample = srcTexture.SampleLevel(samplerState, offsetUV, 0);

            if (sample.a < 1.0f)
            {
                if (bx == 1 || bx == -1 || by == 1 || by == -1)
                {
                    dstTexture[coord] = src;
                    return;
                }
				continue;
            }
            float weight = exp(-(bx * bx) / (2.0f * blurRadiusX * blurRadiusX)
                               -(by * by) / (2.0f * blurRadiusY * blurRadiusY));

            sumR += sample.r * weight;
            sumG += sample.g * weight;
            sumB += sample.b * weight;

            weightSumR += weight;
            weightSumG += weight;
            weightSumB += weight;
        }
    }

    float3 blurred = float3(sumR / weightSumR, sumG / weightSumG, sumB / weightSumB);
    blurred = lerp(src.rgb, blurred, mask.r);
    dstTexture[coord] = float4(blurred, 1.0f);
}
