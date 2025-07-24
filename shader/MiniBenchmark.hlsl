cbuffer ConstBuffer : register(b0)
{
    uint IterationCount;
    uint padding1;
    uint padding2;
    uint padding3;
}

RWTexture2D<float4> dst      : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 threadID : SV_DispatchThreadID)
{
    float acc = threadID.x * 1.0f + threadID.y * 1.0f;
    for (uint i = 0; i < IterationCount; i++)
    {
        acc = sin(acc) * cos(acc) + sqrt(abs(acc));
    }
    dst[threadID.xy] = normalize(float4(0.0f, 0.5f, acc, 1.0f));
}
