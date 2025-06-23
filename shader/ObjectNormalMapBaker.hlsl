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
};

StructuredBuffer<float3> vertices   : register(t0); // a_data.vertices
StructuredBuffer<float2> uvs        : register(t1); // a_data.uvs
StructuredBuffer<float3> normals    : register(t2); // a_data.normals
StructuredBuffer<uint3>  triangles  : register(t3);
StructuredBuffer<uint2>  tileRanges : register(t4);

Texture2D<float4> maskTexture       : register(t5);
RWTexture2D<float4> normalTexture   : register(u0);

bool IsInsideTriangle(float2 p, float2 a, float2 b, float2 c, out float3 bary)
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

    if (abs(denom) < 1e-6)
        return false;

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0 - v - w;

    bary = float3(u, v, w);
    return (u >= 0.0 && v >= 0.0 && w >= 0.0);
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint2 pix = threadID.xy;
    if (pix.x >= texWidth || pix.y >= texHeight)
        return;

    float2 uv = float2(pix) / float2(texWidth, texHeight);
    float3 finalNormal = float3(0, 0, 1); // fallback
    float4 baseColor = float4(0, 0, 1, 1); // fallback

    float4 mask = maskTexture.Load(int3(pix, 0));
    if (mask.a == 0.0)
        return;

    // 삼각형 순회 → 가장 가까운 삼각형 탐색
    [loop]
    for (uint i = 0; i < triangleCount; ++i)
    {
        uint3 tri = triangles[i];
        float2 uv0 = uvs[tri.x];
        float2 uv1 = uvs[tri.y];
        float2 uv2 = uvs[tri.z];

        normalTexture[pix] = float4(0.0f, 0.0f, 1.0f, 1.0f);

        float3 bary;
        if (!IsInsideTriangle(uv, uv0, uv1, uv2, bary))
            continue;

        float3 n0 = normals[tri.x];
        float3 n1 = normals[tri.y];
        float3 n2 = normals[tri.z];

        float3 n = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);

        // 마스크 RGB 기반 보정
        /*float3 axisX = float3(1, 0, 0);
        float3 axisY = float3(0, 1, 0);
        float3 axisZ = float3(0, 0, 1);

        float3 correction = float3(0, 0, 0);
        if (mask.r > 0.0)
        {
            float strength = (mask.r - 0.5) * 2.0;
            correction += strength * ((strength > 0) ? axisX : float3(0, 0, 0));
        }
        if (mask.g > 0.0)
        {
            float strength = (mask.g - 0.5) * 2.0;
            correction += strength * ((strength > 0) ? axisY : float3(0, 0, 0));
        }
        if (mask.b > 0.0)
        {
            float strength = (mask.b - 0.5) * 2.0;
            correction += strength * ((strength > 0) ? axisZ : float3(0, 0, 0));
        }

        n = normalize(lerp(n, correction, length(correction)));*/

        finalNormal = n;
        float3 rgb = finalNormal * 0.5 + 0.5;
        normalTexture[pix] = float4(rgb.r, rgb.b, rgb.g, 1.0);
        return;
    }

    //uint2 pix = threadID.xy + uint2(tileOffsetX, tileOffsetY);
    //if (pix.x >= texWidth || pix.y >= texHeight)
    //    return;

    //uint2 range = tileRanges[tileIndex];
    //uint start = range.x;
    //uint count = range.y;

    //float2 uv = float2(pix) / float2(texWidth, texHeight);
    ////float3 finalNormal = float3(0, 0, 1);

    ///*float4 mask = maskTexture.Load(int3(pix, 0));
    //if (mask.a == 0.0f)
    //    return;*/

    //[loop]
    //for (uint i = 0; i < count; i++)
    //{
    //    uint triIndex = start + i;
    //    uint3 tri = triangles[triIndex];

    //    float2 uv0 = uvs[tri.x];
    //    float2 uv1 = uvs[tri.y];
    //    float2 uv2 = uvs[tri.z];

    //    normalTexture[pix] = float4(0.0f, 0.0f, 1.0f, 1.0f);

    //    float3 bary;
    //    if (!IsInsideTriangle(uv, uv0, uv1, uv2, bary))
    //        continue;

    //    float3 n0 = normals[tri.x];
    //    float3 n1 = normals[tri.y];
    //    float3 n2 = normals[tri.z];

    //    float3 normal = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);

    //    //float3 correction = float3(0, 0, 0);
    //    /*float3 axisX = float3(1, 0, 0);
    //    float3 axisY = float3(0, 1, 0);
    //    float3 axisZ = float3(0, 0, 1);*/

    //    /*if (mask.r > 0.0f)
    //    {
    //        float strength = (mask.r - 0.5f) * 2.0f;
    //        correction += strength * ((strength > 0) ? axisX : float3(0, 0, 0));
    //    }
    //    if (mask.g > 0.0f)
    //    {
    //        float strength = (mask.g - 0.5f) * 2.0f;
    //        correction += strength * ((strength > 0) ? axisY : float3(0, 0, 0));
    //    }
    //    if (mask.b > 0.0f)
    //    {
    //        float strength = (mask.b - 0.5f) * 2.0f;
    //        correction += strength * ((strength > 0) ? axisZ : float3(0, 0, 0));
    //    }*/

    //    //normal = normalize(lerp(normal, correction, length(correction)));
    //    float3 rgb = normal * 0.5f + 0.5f;
    //    normalTexture[pix] = float4(rgb.r, rgb.b, rgb.g, 1.0f);
    //    return;
    //}
}