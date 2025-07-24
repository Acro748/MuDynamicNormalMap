cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    float threshold;
    float blendStrength;
}

Texture2D<float4> srcTexture : register(t0);
RWTexture2D<float4> dstTexture : register(u0);

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
    if (threadID.x >= texWidth || threadID.y >= texHeight)
        return; 

    int2 coord = int2(threadID.xy);
    if (srcTexture.Load(int3(coord, 0)).a < 1.0f)
        return;
    float3 pixel = srcTexture.Load(int3(coord, 0)).rgb;

    float3 sum = 0;
    float weightSum = 0;

    for (uint j = 0; j < 8; j++)
    {
        int2 nearCoord = coord + (offsets[j] * i);

        if (nearCoord.x < 0 || nearCoord.y < 0 ||
            nearCoord.x >= (int)texWidth || nearCoord.y >= (int)texHeight)
            continue;

        float3 neighbor = srcTexture.Load(int3(nearCoord, 0)).rgb;
        float diff = length(neighbor - pixel);

        if (diff > threshold)
        {
            float weight = 1.0 - saturate(diff);
            sum += neighbor * weight;
            weightSum += weight;
        }
    }
    

    if (weightSum > 0)
    {
        float3 smooth = sum / weightSum;
        dstTexture[coord].rgba = float4(lerp(pixel, smooth, blendStrength), 1.0f);
    }
}