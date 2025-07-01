cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint tileSize;
    uint tileCountX;
    uint tileOffsetX;
    uint tileOffsetY;
    uint tileIndex;
    uint triangleCount;
    uint margin;
    uint padding1;
    uint padding2;
    uint padding3;
};

StructuredBuffer<float3> vertices   : register(t0); // a_data.vertices
StructuredBuffer<float2> uvs        : register(t1); // a_data.uvs
StructuredBuffer<float3> normals    : register(t2); // a_data.normals
StructuredBuffer<uint>   indices    : register(t3); // a_data.indices
StructuredBuffer<uint2>  tileRanges : register(t4);

Texture2D<float4> srcTexture        : register(t5);
Texture2D<float4> overlayTexture    : register(t6);

RWTexture2D<float4> dstTexture      : register(u0);

SamplerState samplerState           : register(s0);

float3 Interpolate3(float3 a, float3 b, float3 c, float3 bary)
{
    return normalize(a * bary.x + b * bary.y + c * bary.z);
}

bool ComputeBarycentric(float2 p, float2 a, float2 b, float2 c, out float3 bary)
{
    float2 v0 = b - a;
    float2 v1 = c - a;
    float2 v2 = p - a;

    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);

    float denom = d00 * d11 - d01 * d01;
    if (denom == 0.0f)
        return false;

    bary.y = (d11 * d20 - d01 * d21) / denom;
    bary.z = (d00 * d21 - d01 * d20) / denom;
    bary.x = 1.0 - bary.y - bary.z;

    return all(bary >= 0.0);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    uint2 pix = threadID.xy + uint2(tileOffsetX, tileOffsetY);
    if (pix.x >= texWidth || pix.y >= texHeight)
        return;

    float2 uvPix = (float2(pix) + 0.5) / float2(texWidth, texHeight);
    uint2 range = tileRanges[tileIndex];
    uint start = range.x;
    uint count = range.y;

    float4 dst = float4(0.5f, 0.5f, 1.0f, 1.0f);

    for (uint i = 0; i < count; ++i)
    {
        uint triIndex = start + i;
        uint baseIndex = triIndex * 3;
        uint index0 = indices[baseIndex + 0];
        uint index1 = indices[baseIndex + 1];
        uint index2 = indices[baseIndex + 2];

        float2 u0 = uvs[index0];
        float2 u1 = uvs[index1];
        float2 u2 = uvs[index2];

        float3 bary;
        bool hit = false;

        for (int dy = -int(margin); dy <= int(margin); ++dy)
        {
            for (int dx = -int(margin); dx <= int(margin); ++dx)
            {
                float2 offsetUV = ((float2(pix) + 0.5 + float2(dx, dy)) / float2(texWidth, texHeight));
                if (ComputeBarycentric(offsetUV, u0, u1, u2, bary))
                {
                    hit = true;
                    break;
                }
            }
            if (hit) break;
        }

        if (!hit)
            continue;

        float3 n0 = normals[index0];
        float3 n1 = normals[index1];
        float3 n2 = normals[index2];
        float3 n = Interpolate3(n0, n1, n2, bary);

        float4 srcColor = srcTexture.SampleLevel(samplerState, uvPix, 0);
        float4 overlayColor = overlayTexture.SampleLevel(samplerState, uvPix, 0);

        float3 detailNormal = normalize(srcColor.rgb * 2.0 - 1.0);
        float3 finalNormal = normalize(lerp(n, detailNormal, srcColor.a));
        finalNormal = finalNormal * 0.5 + 0.5;

        dst.rgb = finalNormal.rbg;
        dst.a = 1.0;

        if (overlayColor.a > 0.0)
        {
            dst.rgb = lerp(dst.rgb, overlayColor.rgb, overlayColor.a);
        }

        break; // only paint first hit triangle per pixel
    }

    dstTexture[pix] = dst;
}
