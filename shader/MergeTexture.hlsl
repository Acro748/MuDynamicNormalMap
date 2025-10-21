cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint widthStart;
    uint heightStart;
}

Texture2D<float4> srcTexture : register(t0);
RWTexture2D<float4> dstTexture : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    int2 coord = int2(threadID.xy) + int2(widthStart, heightStart);
    if (coord.x >= (int)texWidth || coord.y >= (int)texHeight)
        return;
    float4 orgColor = dstTexture[coord];
    if (orgColor.a == 1.0f)
        return;
    float4 srcColor = srcTexture.Load(uint3(uint2(coord), 0));
    if (srcColor.a == 0.0f)
        return;
    float4 dstColor = float4(lerp(srcColor.rgb, orgColor.rgb, orgColor.a), 1.0f);
    dstTexture[coord] = dstColor;
}
