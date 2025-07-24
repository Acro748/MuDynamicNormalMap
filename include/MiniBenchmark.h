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

    inline float miniBenchMarkGPU() {
        HRESULT hr;
        auto device = Shader::ShaderManager::GetSingleton().GetDevice();
        auto context = Shader::ShaderManager::GetSingleton().GetContext();
        if (!device || !context)
        {
            logger::error("Unable to get device / context");
            return -1.0f;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        desc.MiscFlags = 0;
        desc.MipLevels = 1;
        desc.CPUAccessFlags = 0;
        desc.ArraySize = 1;
        desc.Width = 4096;
        desc.Height = 4096;
        desc.SampleDesc.Count = 1;
        hr = device->CreateTexture2D(&desc, nullptr, &texture2D);
        if (FAILED(hr))
        {
            logger::error("{} : Failed to create texture 2d ({})", __func__, hr);
            return -1.0f;
        }

        Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> textureUAV;
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = 0;
        hr = device->CreateUnorderedAccessView(texture2D.Get(), &uavDesc, &textureUAV);
        if (FAILED(hr))
        {
            logger::error("{} : Failed to create unordered access view ({})", __func__, hr);
            return -1.0f;
        }

        struct ConstBufferData
        {
            UINT IterationCount;
            UINT padding1;
            UINT padding2;
            UINT padding3;
        };
        static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");

        ConstBufferData cbData = {};
        cbData.IterationCount = 10;

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(ConstBufferData);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        D3D11_SUBRESOURCE_DATA cbInitData = {};
        cbInitData.pSysMem = &cbData;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
        hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
        if (FAILED(hr)) {
            logger::error("{} : Failed to create const buffer ({})", __func__, hr);
            return -1.0f;
        }

		auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader("MiniBenchmark");
        if (!shader)
        {
            logger::error("{} : Invalid shader", __func__);
            return -1.0f;
		}

        struct ShaderBackup {
        public:
            void Backup(ID3D11DeviceContext* context) {
                context->CSGetShader(&shader, nullptr, 0);
                context->CSGetConstantBuffers(0, 1, &constBuffer);
                context->CSGetUnorderedAccessViews(0, 1, &UAV);
            }
            void Revert(ID3D11DeviceContext* context) {
                context->CSSetShader(shader.Get(), nullptr, 0);
                context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
                context->CSSetUnorderedAccessViews(0, 1, UAV.GetAddressOf(), nullptr);
            }
            Shader::ShaderManager::ComputeShader shader;
            Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> UAV;
        private:
        } sb;

        DirectX::XMUINT2 dispatch = { (desc.Width + 8 - 1) / 8, (desc.Height + 8 - 1) / 8 };

        Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
        D3D11_QUERY_DESC qDesc = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
        device->CreateQuery(&qDesc, &disjointQuery);

        Microsoft::WRL::ComPtr<ID3D11Query> startQuery, endQuery;
        qDesc.Query = D3D11_QUERY_TIMESTAMP;
        device->CreateQuery(&qDesc, &startQuery);
        device->CreateQuery(&qDesc, &endQuery);

        Shader::ShaderManager::GetSingleton().ShaderContextLock();
        context->Begin(disjointQuery.Get());
        context->End(startQuery.Get());
        sb.Backup(context);
        context->CSSetShader(shader.Get(), nullptr, 0);
        context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
        context->CSSetUnorderedAccessViews(0, 1, textureUAV.GetAddressOf(), nullptr);
        context->Dispatch(dispatch.x, dispatch.y, 1);
        sb.Revert(context);
        context->End(endQuery.Get());
        context->End(disjointQuery.Get());
        Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

        while (true) {
            Shader::ShaderManager::GetSingleton().ShaderContextLock();
            bool isDoing = context->GetData(disjointQuery.Get(), nullptr, 0, 0) == S_FALSE;
            Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
            if (!isDoing)
				break;
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData = {};
        Shader::ShaderManager::GetSingleton().ShaderContextLock();
        context->GetData(disjointQuery.Get(), &disjointData, sizeof(disjointData), 0);
        Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

        if (!disjointData.Disjoint)
        {
            UINT64 startTime = 0, endTime = 0;
            Shader::ShaderManager::GetSingleton().ShaderContextLock();
            context->GetData(startQuery.Get(), &startTime, sizeof(UINT64), 0);
            context->GetData(endQuery.Get(), &endTime, sizeof(UINT64), 0);
            Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

            double timeMS = (endTime - startTime) / double(disjointData.Frequency) * 1000000.0;
            return 50000.0 / timeMS;
        }
        return -1.0f;
    }
}