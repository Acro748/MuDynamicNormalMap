cbuffer ObjectTransform : register(b0)
{
    float3 centerOffset;
    float padding;
    float4x4 rotationMatrix;
}

// Inputs
StructuredBuffer<float3> inPositions  : register(t0);
StructuredBuffer<float3> inNormals    : register(t1);
StructuredBuffer<float2> inUVs        : register(t2);
StructuredBuffer<uint3>  inIndices    : register(t3);

// Output
Texture2D<float4> srcTexture : register(t4);
Texture2D<float4> maskTexture : register(t5);
RWTexture2D<float4> normalMapOut : register(u0);

float3 BarycentricCoord(float2 p, float2 a, float2 b, float2 c)
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
        return float3(-1, -1, -1); // degenerate triangle

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;
    return float3(u, v, w);
}

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint triIndex = tid.x;

    // === 삼각형 정보 ===
    uint3 idx = inIndices[triIndex];

    float3 p0 = inPositions[idx.x];
    float3 p1 = inPositions[idx.y];
    float3 p2 = inPositions[idx.z];

    float3 n0 = normalize(inNormals[idx.x]);
    float3 n1 = normalize(inNormals[idx.y]);
    float3 n2 = normalize(inNormals[idx.z]);

    float2 uv0 = inUVs[idx.x];
    float2 uv1 = inUVs[idx.y];
    float2 uv2 = inUVs[idx.z];

    // === UV → 텍스처 좌표 ===
    uint2 texSize;
    normalMapOut.GetDimensions(texSize.x, texSize.y);

    float2 screen0 = uv0 * texSize;
    float2 screen1 = uv1 * texSize;
    float2 screen2 = uv2 * texSize;

    // === 삼각형 영역 내의 픽셀 반복 ===
    float2 minUV = floor(min(min(screen0, screen1), screen2));
    float2 maxUV = ceil(max(max(screen0, screen1), screen2));

    [loop]
    for (uint y = (uint)minUV.y; y <= (uint)maxUV.y; ++y)
    {
        [loop]
        for (uint x = (uint)minUV.x; x <= (uint)maxUV.x; ++x)
        {
            float2 p = float2(x + 0.5f, y + 0.5f); // 픽셀 중심

            float3 bary = BarycentricCoord(p, screen0, screen1, screen2);
            if (bary.x < 0 || bary.y < 0 || bary.z < 0) continue;

            // === 위치/노멀 보간 ===
            float3 pos = (p0 * bary.x + p1 * bary.y + p2 * bary.z) - centerOffset;
            float3 normal = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);

            // === 회전 적용 (예: Blender → Skyrim 좌표계 변환) ===
            pos = mul((float3x3)rotationMatrix, pos);
            normal = mul((float3x3)rotationMatrix, normal);

            // === 오브젝트 공간 노멀 → RGB 인코딩 ===
            float3 encoded = normal * 0.5f + 0.5f;

            // === 출력 ===
            float4 original = srcTexture.Load(int3(x, y, 0));
            float4 mask = maskTexture.Load(int3(x, y, 0)); 
            float maskAlpha = saturate(mask.a);
            float3 final = lerp(original.rgb, encoded, maskAlpha);
            normalMapOut[uint2(x, y)] = float4(final, 1.0f);
            //normalMapOut[uint2(x, y)] = float4(1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}
