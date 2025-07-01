#pragma once

namespace Mus {
    inline float compute(float x) {
        DirectX::XMVECTOR v1 = DirectX::XMVectorSet(x, x + 1.0f, x + 2.0f, x + 3.0f);
        DirectX::XMVECTOR v2 = DirectX::XMVectorSet(1.0f, 2.0f, 3.0f, 4.0f);
        DirectX::XMVECTOR result = v1;
        DirectX::XMMATRIX matrix(v1, v2, result, DirectX::XMVectorSet(0, 0, 0, 1));

        for (int i = 0; i < 10; i++) {
            result = DirectX::XMVectorAdd(result, v2);
            result = DirectX::XMVectorSubtract(result, v1);
            result = DirectX::XMVectorMultiply(result, v2);
            result = DirectX::XMVectorDivide(result, v1);

            result = DirectX::XMVector3Normalize(result);
            result = DirectX::XMVector3Dot(result, v1);
            result = DirectX::XMVector3Cross(result, v2);

            DirectX::XMVECTOR resultAlt;
            if (DirectX::XMVector3Equal(result, v1))
            {
                resultAlt = DirectX::XMVectorScale(result, x);
            }
            else
            {
                resultAlt = DirectX::XMVectorScale(result, x * 2);
            }

            result = DirectX::XMVectorLerp(result, resultAlt, x);
            result = DirectX::XMVector3TransformNormal(result, matrix);
        }

        return DirectX::XMVectorGetX(result);
    }

    inline float miniBenchMark() {
        constexpr std::size_t benchMarkStrength = 10000000;
        concurrency::concurrent_vector<float> data(benchMarkStrength, 1.0);

        auto start = std::chrono::high_resolution_clock::now();
        concurrency::parallel_for(std::size_t(0), benchMarkStrength, [&data](std::size_t i) {
            data[i] = compute(data[i]);
        });
        std::atomic_signal_fence(std::memory_order_seq_cst);
        logger::trace("Test {}", data[benchMarkStrength - 1]);

        auto end = std::chrono::high_resolution_clock::now();
        float seconds = std::chrono::duration<float>(end - start).count();
        float mops = static_cast<float>(benchMarkStrength) / seconds / 1000000.0;

        return mops;
    }
}