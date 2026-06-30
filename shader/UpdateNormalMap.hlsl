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
    uint vertexEnd;
    uint padding1;
};

StructuredBuffer<float3> vertices   : register(t0); // a_data->vertices
StructuredBuffer<float2> uvs        : register(t1); // a_data->uvs
StructuredBuffer<float3> normals    : register(t2); // a_data->normals
StructuredBuffer<float3> tangent    : register(t3); // a_data->tangent
StructuredBuffer<float3> bitangent  : register(t4); // a_data->bitangent
StructuredBuffer<uint>   indices    : register(t5); // a_data->indices

Texture2D<float4> srcTexture        : register(t6);
Texture2D<float4> detailTexture     : register(t7);
Texture2D<float4> overlayTexture    : register(t8);
Texture2D<float4> maskTexture       : register(t9);

RWTexture2D<float4> dstTexture      : register(u0);

SamplerState samplerState           : register(s0);

float EdgeFunction(float2 a, float2 b, float2 c)
{
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

bool IsTopLeft(float2 a, float2 b)
{
    float2 e = b - a;
    return (e.y > 0) || (e.y == 0 && e.x < 0);
}

[numthreads(64, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    const uint index = indicesStart + threadID.x * 3;
    if (index + 2 >= indicesEnd)
        return;

    uint i0 = indices[index + 0];
    uint i1 = indices[index + 1];
    uint i2 = indices[index + 2];

    if (i0 >= vertexEnd || i1 >= vertexEnd || i2 >= vertexEnd)
        return;
    //float3 v0 = vertices[i0];
    //float3 v1 = vertices[i1];
    //float3 v2 = vertices[i2];

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

    float area = EdgeFunction(p0, p1, p2);
    if (abs(area) < 1e-6)
        return;
	float invArea = rcp(area);
	bool isCCW = area > 0.0f;

    float minX = floor(min(min(p0.x, p1.x), p2.x));
    float minY = floor(min(min(p0.y, p1.y), p2.y));
    float maxX = ceil(max(max(p0.x, p1.x), p2.x));
    float maxY = ceil(max(max(p0.y, p1.y), p2.y));

    float2 minBB = min(p0, min(p1, p2));
    float2 maxBB = max(p0, max(p1, p2));
    int2 minP = (int2)floor(minBB);
    int2 maxP = (int2)ceil(maxBB);
    if (any((maxP - minP) >= int2(texWidth * 2, texHeight * 2)))
        return;

    float2 invTexSize = rcp(float2(texWidth, texHeight));

    for (int y = minP.y; y <= maxP.y; y++)
    {
        for (int x = minP.x; x <= maxP.x; x++)
        {
            float2 xy = float2((float)x, (float)y);
            float2 p = xy + float2(0.5f, 0.5f);

            float w0 = EdgeFunction(p1, p2, p);
            float w1 = EdgeFunction(p2, p0, p);
            float w2 = EdgeFunction(p0, p1, p);

            if (isCCW)
            {
                if (!(w0 > 0 || (w0 == 0 && IsTopLeft(p1, p2)))) 
                    continue;
                if (!(w1 > 0 || (w1 == 0 && IsTopLeft(p2, p0)))) 
                    continue;
                if (!(w2 > 0 || (w2 == 0 && IsTopLeft(p0, p1)))) 
                    continue;
            }
            else
            {
                float cw0 = -w0;
                float cw1 = -w1;
                float cw2 = -w2;
                if (!(cw0 > 0 || (cw0 == 0 && IsTopLeft(p2, p1)))) 
                    continue;
                if (!(cw1 > 0 || (cw1 == 0 && IsTopLeft(p0, p2)))) 
                    continue;
                if (!(cw2 > 0 || (cw2 == 0 && IsTopLeft(p1, p0))))
                    continue;
			}

            float3 bary = float3(w0, w1, w2) * invArea;
            float2 uv = xy * invTexSize;
            float4 dstColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
            float4 overlayColor = float4(1.0f, 1.0f, 1.0f, 0.0f);
            if (hasOverlayTexture > 0)
            {
                overlayColor = overlayTexture.SampleLevel(samplerState, uv, 0);
            }
            if (overlayColor.a < 1.0f)
            {
                float3 n = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
                float4 maskColor = float4(0.5f, 0.5f, 0.5f, 0.0f);
                if (hasMaskTexture > 0 && hasSrcTexture > 0)
                {
                    maskColor = maskTexture.SampleLevel(samplerState, uv, 0);
                }
                if (maskColor.a < 1.0f)
                {
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

                        float3 ft = normalize(t - n * dot(n, t));
                        float handedness = dot(cross(n, t), b) < 0.0f ? -1.0f : 1.0f;
                        float3 fb = normalize(cross(n, ft)) * handedness;
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

            uint wrapX = (uint)x & (texWidth - 1);
            uint wrapY = (uint)y & (texHeight - 1);
            uint2 wrapXY = uint2(wrapX, wrapY);
            dstTexture[wrapXY] = float4(dstColor.rgb, 1.0f);
        }
    }
}
