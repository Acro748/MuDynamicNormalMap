cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint indicesStart;
    uint indicesEnd;

    uint hasSrcTexture;
    uint hasDetailTexture;
    uint hasOverlayTexture;
    uint hasMaskTexture;

    uint tangentZCorrection;
    float detailStrength;
    uint padding1;
    uint padding2;
};

StructuredBuffer<float3> vertices   : register(t0); // a_data.vertices
StructuredBuffer<float2> uvs        : register(t1); // a_data.uvs
StructuredBuffer<float3> normals    : register(t2); // a_data.normals
StructuredBuffer<float3> tangent    : register(t3); // a_data.tangent
StructuredBuffer<float3> bitangent  : register(t4); // a_data.bitangent
StructuredBuffer<uint>   indices    : register(t5); // a_data.indices

Texture2D<float4> srcTexture        : register(t6);
Texture2D<float4> detailTexture     : register(t7);
Texture2D<float4> overlayTexture    : register(t8);
Texture2D<float4> maskTexture       : register(t9);

RWTexture2D<float4> dstTexture      : register(u0);
RWByteAddressBuffer pixelLock       : register(u1);

SamplerState samplerState           : register(s0);

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

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    if (u < 0 || v < 0 || w < 0)
        return false;

    bary = float3(u, v, w);
    return true;
}

[numthreads(64, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    const uint index = indicesStart+ threadID.x * 3;
    if (index + 2 >= indicesEnd)
        return;

    uint i0 = indices[index + 0];
    uint i1 = indices[index + 1];
    uint i2 = indices[index + 2];

    float2 uv0 = uvs[i0];
    float2 uv1 = uvs[i1];
    float2 uv2 = uvs[i2];

    float3 n0 = normals[i0];
    float3 n1 = normals[i1];
    float3 n2 = normals[i2];

    float3 t0 = tangent[i0];
    float3 t1 = tangent[i1];
    float3 t2 = tangent[i2];

    float3 b0 = bitangent[i0];
    float3 b1 = bitangent[i1];
    float3 b2 = bitangent[i2];

    float2 p0 = uv0 * float2(texWidth, texHeight);
    float2 p1 = uv1 * float2(texWidth, texHeight);
    float2 p2 = uv2 * float2(texWidth, texHeight);

    float minX = floor(min(min(p0.x, p1.x), p2.x));
    float minY = floor(min(min(p0.y, p1.y), p2.y));
    float maxX = ceil(max(max(p0.x, p1.x), p2.x));
    float maxY = ceil(max(max(p0.y, p1.y), p2.y));

    int2 minP = clamp(int2(minX, minY), int2(0, 0), int2(texWidth - 1, texHeight - 1));
    int2 maxP = clamp(int2(maxX, maxY), int2(0, 0), int2(texWidth - 1, texHeight - 1));

    for (int y = minP.y; y < maxP.y; y++)
    {
        for (int x = minP.x; x < maxP.x; x++)
        {
            uint2 xy = uint2(x, y);
            float3 bary;
            if (!ComputeBarycentric(float2(xy) + float2(0.5f, 0.5f), p0, p1, p2, bary))
                continue;

            uint addr = (y * texWidth + x) * 4;
            uint pixelLockFlag;
            pixelLock.InterlockedCompareExchange(addr, 0, 1, pixelLockFlag);
            if (pixelLockFlag != 0)
                continue;

            float2 uv = float2(xy) / float2(texWidth, texHeight);

            float4 dstColor;
            float4 overlayColor = float4(1.0f, 1.0f, 1.0f, 0.0f);
            if (hasOverlayTexture > 0)
            {
                overlayColor = overlayTexture.SampleLevel(samplerState, uv, 0);
            }

            if (overlayColor.a < 1.0f)
            {
                float4 maskColor = float4(1.0f, 1.0f, 1.0f, 0.0f);
                if (hasMaskTexture > 0 && hasSrcTexture > 0)
                {
                    maskColor = maskTexture.SampleLevel(samplerState, uv, 0);
                }
                if (maskColor.a < 1.0f)
                {
                    float3 n = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);

                    float4 detailColor = float4(0.5f, 0.5f, 1.0f, 0.5f);
                    if (hasDetailTexture > 0)
                    {
                        detailColor = detailTexture.SampleLevel(samplerState, uv, 0);
                        detailColor = lerp(float4(0.5f, 0.5f, 1.0f, detailColor.a), detailColor, detailStrength);
                    }

                    float3 normalResult;
                    if (detailColor.a > 0.0f)
                    {
                        float3 t = normalize(t0 * bary.x + t1 * bary.y + t2 * bary.z);
                        float3 b = normalize(b0 * bary.x + b1 * bary.y + b2 * bary.z);

                        float3 ft = normalize(t - n * dot(n, t).x);
                        float3 fb = normalize(cross(n, ft));

                        float3x3 tbn = float3x3(ft, fb, n);

                        float3 srcN = float3(detailColor.rgb * 2.0f - 1.0f);
                        if (tangentZCorrection)
                        {
                            srcN.z = sqrt(max(0.0f, 1.0f - srcN.x * srcN.x - srcN.y * srcN.y));
                        }

                        float3 detailNormal = normalize(mul(srcN, tbn));
                        normalResult = normalize(lerp(n, detailNormal, detailColor.a));
                    }
                    else
                    {
                        normalResult = n;
                    }

                    float3 finalNormal = normalResult * 0.5f + 0.5f;
                    dstColor.rgb = finalNormal.xzy;
                }
                if (maskColor.a > 0.0f && hasSrcTexture > 0)
                {
					float4 srcColor = srcTexture.SampleLevel(samplerState, uv, 0);
					dstColor.rgb = lerp(dstColor.rgb, srcColor.rgb, maskColor.a);
                }
            }

            if (overlayColor.a > 0.0f)
            {
                dstColor.rgb = lerp(dstColor.rgb, overlayColor.rgb, overlayColor.a);
            }

            dstTexture[xy] = float4(dstColor.rgb, 1.0f);
        }
    }
}
