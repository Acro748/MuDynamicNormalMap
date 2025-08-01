cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint widthStart;
    uint heightStart;

    int searchRadius;
    uint padding1;
    uint padding2;
    uint padding3;
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
    uint2 coord = int2(threadID.xy) + int2(widthStart, heightStart);
    if (coord.x >= texWidth || coord.y >= texHeight)
        return; 

	float2 uv = (float2(coord) + 0.5f) / float2(texWidth, texHeight);
    float4 srcColor = srcTexture.SampleLevel(samplerState, uv, 0);
    float maskColor = maskTexture.SampleLevel(samplerState, uv, 0).r;
    if (maskColor <= 0.0001f)
    {
        dstTexture[coord] = srcColor;
        return;
    }

    if (srcColor.a < 1.0f)
        return;

    float4 nearestColor = float4(0, 0, 0, 0);
    bool found = false;
    bool end = false;

    for (int offsetMult = 1; offsetMult < searchRadius; offsetMult++)
    {
        for (int i = 0; i < 8; i++) {
            int2 nearCoord = (int)coord + offsets[i] * offsetMult;
            if (nearCoord.x < 0 || nearCoord.y < 0 ||
                nearCoord.x >= (int)texWidth || nearCoord.y >= (int)texHeight)
            {
                end = true;
                break;
            }
            float2 nearUV = float2(nearCoord) / float2(texWidth, texHeight);
            float4 nearSrc = srcTexture.SampleLevel(samplerState, nearUV, 0);
            if (nearSrc.a < 1.0f)
            {
                end = true;
                break;
            }
            float nearMask = maskTexture.SampleLevel(samplerState, nearUV, 0).r;
            if (nearMask <= 0.0001f)
            {
                nearestColor = nearSrc;
                found = true;
                break;
            }
        }
        if (end || found)
            break;
    }
    if (!found)
    {
        dstTexture[coord] = srcColor;
        return;
    }

    dstTexture[coord] = float4(lerp(srcColor.rgb, nearestColor.rgb, maskColor), 1.0f);
}
