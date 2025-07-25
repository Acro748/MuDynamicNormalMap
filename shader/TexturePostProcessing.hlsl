cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint widthStart;
    uint heightStart;
    float threshold;
    float blendStrength;
}

Texture2D<float4> srcTexture : register(t0);
RWTexture2D<float4> dstTexture : register(u0);

static const int2 offsets[4] = {
    int2(-1, -1), // left up
    int2(0, -1), // up
    int2(1, -1), // right up
    int2(-1,  0), // left
};

[numthreads(64, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    int2 coord = int2(threadID.xy) + int2(widthStart, heightStart);
    if (coord.x >= texWidth || coord.y >= texHeight)
        return; 

    if (srcTexture.Load(int3(coord, 0)).a < 1.0f)
        return;
    float3 pixel = srcTexture.Load(int3(coord, 0)).rgb;

    float3 sum = 0;
    float weightSum = 0;
	int validCount = 0;
    for (uint i = 0; i < 4; i++)
    {
        int2 nearCoord = coord + offsets[i];

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
            validCount++;
        }
    }

    if (weightSum > 0 && validCount >= 3)
    {
        float3 smooth = sum / weightSum;
        dstTexture[coord].rgba = float4(lerp(pixel, smooth, blendStrength), 1.0f);
    }
}