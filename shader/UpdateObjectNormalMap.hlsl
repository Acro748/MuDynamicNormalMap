cbuffer ConstBuffer : register(b0)
{
    uint texWidth;
    uint texHeight;
    uint indicesOffset;
    uint indicesMax;
    uint srcWidth;
    uint srcHeight;
    uint overlayWidth;
    uint overlayHeight;
};

StructuredBuffer<float3> vertices   : register(t0); // a_data.vertices
StructuredBuffer<float2> uvs        : register(t1); // a_data.uvs
StructuredBuffer<float3> normals    : register(t2); // a_data.normals
StructuredBuffer<float3> tangent    : register(t3); // a_data.tangent
StructuredBuffer<float3> bitangent  : register(t4); // a_data.bitangent
StructuredBuffer<uint>   indices    : register(t5); // a_data.indices

Texture2D<float4> srcTexture        : register(t6);
Texture2D<float4> overlayTexture    : register(t7);

RWTexture2D<float4> dstTexture      : register(u0);
RWByteAddressBuffer flagsBuffer     : register(u1);

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

    bary.y = (d11 * d20 - d01 * d21) / denom;
    bary.z = (d00 * d21 - d01 * d20) / denom;
    bary.x = 1.0 - bary.y - bary.z;

    return all(bary >= 0.0);
}

bool ComputeBarycentrics(float2 p, float2 a, float2 b, float2 c, out float3 bary)
{
    return ComputeBarycentric(p, a, b, c, bary) || ComputeBarycentric(p + float2(0.0f, 1.0f), a, b, c, bary) ||
        ComputeBarycentric(p + float2(1.0f, 0.0f), a, b, c, bary) || ComputeBarycentric(p + float2(1.0f, 1.0f), a, b, c, bary);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    const uint index = indicesOffset + threadID.x * 3;
    if (index + 2 >= indicesMax)
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

    int2 minP = int2(floor(min(min(p0, p1), p2)));
    int2 maxP = int2(ceil(max(max(p0, p1), p2)));

    minP = clamp(minP, int2(0, 0), int2(texWidth - 1, texHeight - 1));
    maxP = clamp(maxP, int2(0, 0), int2(texWidth - 1, texHeight - 1));

    for (int y = minP.y; y <= maxP.y; y++)
    {
        for (int x = minP.x; x <= maxP.x; x++)
        {
            uint pixelIndex = (y * texWidth + x);
            uint pixelFlag;
            flagsBuffer.InterlockedCompareExchange(pixelIndex, 0, 1, pixelFlag);
            if (pixelFlag != 0)
                continue;

            float2 uv = float2(x, y) / float2(texWidth, texHeight);
            float3 bary;
            if (!ComputeBarycentrics(uv, uv0, uv1, uv2, bary))
                continue;

            float3 n = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
            float3 t = normalize(t0 * bary.x + t1 * bary.y + t2 * bary.z);
            float3 b = normalize(b0 * bary.x + b1 * bary.y + b2 * bary.z);

            float3 ft = normalize(t - n * dot(n, t));
            float3 crossN = cross(n, ft);
            float handedness = (dot(crossN, b) < 0.0f) ? -1.0f : 1.0f;
            float3 fb = normalize(crossN * handedness);

            float3x3 tbn = float3x3(ft, fb, n);

            float4 srcColor = float4(0.5f, 0.5f, 1.0f, 0.0f);
            float4 overlayColor = float4(1.0f, 1.0f, 1.0f, 0.0f);

            if (srcWidth > 0 && srcHeight > 0)
            {
                float2 srcUV = float2(x, y) / float2(srcWidth, srcHeight);
                srcUV = clamp(srcUV, int2(0, 0), int2(srcWidth - 1, srcHeight - 1));
                srcColor = srcTexture.SampleLevel(samplerState, srcUV, 0);
            }
            if (overlayWidth > 0 && overlayHeight > 0)
            {
                float2 overlayUV = float2(x, y) / float2(overlayWidth, overlayHeight);
                overlayUV = clamp(overlayUV, int2(0, 0), int2(overlayWidth - 1, overlayHeight - 1));
                overlayColor = overlayTexture.SampleLevel(samplerState, overlayUV, 0);
            }

            float3 srcNormal = normalize(mul(tbn, srcColor.rgb * 2.0f - 1.0f));
            float3 blendedNormal = normalize(lerp(n, srcNormal, srcColor.a));
            float3 packedNormal = blendedNormal * 0.5f + 0.5f;

            float4 finalColor = float4(packedNormal.r, packedNormal.b, packedNormal.g, 1.0f);

            if (overlayColor.a > 0.0f)
            {
                finalColor.rgb = lerp(finalColor.rgb, overlayColor.rgb, overlayColor.a);
            }

            dstTexture[int2(x, y)] = finalColor;
        }
    }
}
