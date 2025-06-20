#include "ObjectNormalMapBaker.h"

namespace Mus {
	RE::NiPointer<RE::NiSourceTexture> ObjectNormalMapBaker::BakeObjectNormalMap(TaskID taskID, std::string textureName, GeometryData a_data, std::string a_srcTexturePath, std::string a_maskTexturePath)
	{
		PerformanceLog(std::string(__func__) + "::" + textureName, false, false);

		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| textureName.empty())
		{
			logger::error("{} : Invalid parameters", __func__);
			return nullptr;
		}

		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
			return nullptr;

		a_data.Subdivision(Config::GetSingleton().GetSubdivision());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
			return nullptr;

		a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
			return nullptr;

		a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
			return nullptr;

		HRESULT hr;
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
		{
			logger::error("{} : Invalid renderer", __func__);
			return nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> maskStagingTexture2D, dstStagingTexture2D;
		D3D11_TEXTURE2D_DESC dstDesc = {}, dstStagingDesc = {}, maskStagingDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};
		dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstDesc.Usage = D3D11_USAGE_DEFAULT;
		dstDesc.BindFlags = 0;
		dstDesc.MiscFlags = 0;
		dstDesc.Width = Config::GetSingleton().GetDefaultTextureWidth();
		dstDesc.Height = Config::GetSingleton().GetDefaultTextureHeight();
		dstDesc.CPUAccessFlags = 0;

		dstStagingDesc = dstDesc;
		dstStagingDesc.Usage = D3D11_USAGE_STAGING;
		dstStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

		dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

		if (!a_srcTexturePath.empty())
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
			D3D11_TEXTURE2D_DESC srcDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
			if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(a_srcTexturePath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM,
																		Config::GetSingleton().GetIgnoreTextureSize() ? Config::GetSingleton().GetDefaultTextureWidth() : 0,
																		Config::GetSingleton().GetIgnoreTextureSize() ? Config::GetSingleton().GetDefaultTextureHeight() : 0,
																		srcTexture2D))
			{
				if (!Config::GetSingleton().GetIgnoreTextureSize() && Config::GetSingleton().GetTextureResize() != 1.0f)
				{
					if (!Shader::TextureLoadManager::GetSingleton().ConvertTexture(srcTexture2D, DXGI_FORMAT_UNKNOWN, srcDesc.Width * Config::GetSingleton().GetTextureResize(), srcDesc.Height * Config::GetSingleton().GetTextureResize(),
																				   DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, srcTexture2D))
					{
						logger::error("{} : Failed to resize texture", __func__);
					}
					else
					{
						srcDesc.Width = srcDesc.Width * Config::GetSingleton().GetTextureResize();
						srcDesc.Height = srcDesc.Height * Config::GetSingleton().GetTextureResize();
					}
				}

				dstDesc = srcDesc;
				dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

				dstStagingDesc = srcDesc;
				dstStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstStagingDesc.Usage = D3D11_USAGE_STAGING;
				dstStagingDesc.BindFlags = 0;
				dstStagingDesc.MiscFlags = 0;
				dstStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

				dstShaderResourceViewDesc = srcShaderResourceViewDesc;
				dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

				hr = device->CreateTexture2D(&dstStagingDesc, nullptr, &dstStagingTexture2D);
				if (FAILED(hr))
				{
					logger::error("{} : Failed to create dst staging texture ({})", __func__, hr);
					return nullptr;
				}

				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->CopyResource(dstStagingTexture2D.Get(), srcTexture2D.Get());
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
		}

		if (!a_maskTexturePath.empty())
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> maskTexture2D = nullptr;
			D3D11_TEXTURE2D_DESC maskDesc;
			if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(a_maskTexturePath, maskDesc, DXGI_FORMAT_R8G8B8A8_UNORM, maskTexture2D))
			{
				maskStagingDesc = maskDesc;
				maskStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				maskStagingDesc.Usage = D3D11_USAGE_STAGING;
				maskStagingDesc.BindFlags = 0;
				maskStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				maskStagingDesc.MiscFlags = 0;
				hr = device->CreateTexture2D(&maskStagingDesc, nullptr, &maskStagingTexture2D);
				if (FAILED(hr))
				{
					logger::error("{} : Failed to create src staging texture ({})", __func__, hr);
					maskStagingTexture2D = nullptr;
				}
				else
				{
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					context->CopyResource(maskStagingTexture2D.Get(), maskTexture2D.Get());
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					if (FAILED(hr))
					{
						logger::error("{} : Failed to map mask staging texture ({})", __func__, hr);
						maskStagingTexture2D = nullptr;
					}
				}
			}
		}

		if (!dstStagingTexture2D)
		{
			hr = device->CreateTexture2D(&dstStagingDesc, nullptr, &dstStagingTexture2D);
			if (FAILED(hr))
			{
				logger::error("{} : Failed to create dst staging texture ({})", __func__, hr);
				return nullptr;
			}
		}

		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
			return nullptr;

		concurrency::SchedulerPolicy policy = concurrency::CurrentScheduler::GetPolicy();
		policy.SetPolicyValue(
			concurrency::ContextPriority, THREAD_PRIORITY_BELOW_NORMAL
		);
		concurrency::CurrentScheduler::Create(policy);

		logger::info("{} : {} {} {} {} baking normalmap...", __func__, a_data.vertices.size(), a_data.uvs.size(), a_data.normals.size(), a_data.indices.size());

		std::size_t totalTaskCount = a_data.indices.size() / 3;

		D3D11_MAPPED_SUBRESOURCE maskMappedResource;
		if (maskStagingTexture2D)
		{
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			hr = context->Map(maskStagingTexture2D.Get(), 0, D3D11_MAP_READ, 0, &maskMappedResource);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		}

		std::vector<std::thread> parallelMips;
		for (UINT mip = 0; mip < dstStagingDesc.MipLevels; mip++)
		{
			parallelMips.push_back(std::thread([&, mip]() {
				TaskManager::SetDeferredWorker();
				D3D11_MAPPED_SUBRESOURCE mappedResource;
				UINT subresourceIndex = D3D11CalcSubresource(mip, 0, dstStagingDesc.MipLevels);
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(dstStagingTexture2D.Get(), subresourceIndex, D3D11_MAP_READ_WRITE, 0, &mappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				if (FAILED(hr))
				{
					logger::error("Failed to read data from the staging texture ({})", hr);
					return nullptr;
				}

				UINT mipWidth = (std::max)(UINT(1), dstStagingDesc.Width >> mip);
				UINT mipHeight = (std::max)(UINT(1), dstStagingDesc.Height >> mip);
				uint8_t* data = reinterpret_cast<uint8_t*>(mappedResource.pData);
				uint8_t* maskData = reinterpret_cast<uint8_t*>(maskMappedResource.pData);

				std::size_t chunkSize = 16;
				if (mip > 0)
					chunkSize *= std::pow(2, mip + 1);
				std::size_t chunkCount = (totalTaskCount + chunkSize - 1) / chunkSize;
				concurrency::parallel_for(std::size_t(0), std::size_t(chunkCount), [&](std::size_t taskIndex) {
					//PerformanceLog(std::string(__func__) + "::" + textureName + "::" + std::to_string(taskIndex), false, false);
					std::size_t start = taskIndex * chunkSize;
					std::size_t end = (std::min)(start + chunkSize, totalTaskCount);
					for (std::size_t i = start; i < end; i++)
					{
						std::size_t index = i * 3;

						std::size_t index1 = a_data.indices[index + 0];
						std::size_t index2 = a_data.indices[index + 1];
						std::size_t index3 = a_data.indices[index + 2];

						DirectX::XMFLOAT3& v0 = a_data.vertices[index1];
						DirectX::XMFLOAT3& v1 = a_data.vertices[index2];
						DirectX::XMFLOAT3& v2 = a_data.vertices[index3];

						DirectX::XMFLOAT2& u0 = a_data.uvs[index1];
						DirectX::XMFLOAT2& u1 = a_data.uvs[index2];
						DirectX::XMFLOAT2& u2 = a_data.uvs[index3];

						DirectX::XMFLOAT3& n0 = a_data.normals[index1];
						DirectX::XMFLOAT3& n1 = a_data.normals[index2];
						DirectX::XMFLOAT3& n2 = a_data.normals[index3];

						auto uvToPix = [&](DirectX::XMFLOAT2 uv) -> DirectX::XMINT2 {
							return {
								static_cast<int>(uv.x * mipWidth),
								static_cast<int>(uv.y * mipHeight)
							};
							};

						auto p0 = uvToPix(u0);
						auto p1 = uvToPix(u1);
						auto p2 = uvToPix(u2);

						int minX = (std::min)({ p0.x, p1.x, p2.x });
						int minY = (std::min)({ p0.y, p1.y, p2.y });
						int maxX = (std::max)({ p0.x, p1.x, p2.x });
						int maxY = (std::max)({ p0.y, p1.y, p2.y });

						for (int y = minY; y <= maxY; ++y)
						{
							for (int x = minX; x <= maxX; ++x)
							{
								std::uint8_t* rowData = static_cast<std::uint8_t*>(data) + y * mappedResource.RowPitch;
								std::uint32_t* srcPixel = reinterpret_cast<uint32_t*>(rowData + x * 4);

								RGBA srcColor;
								srcColor.SetReverse(*srcPixel);

								RGBA maskColor;
								maskColor.a = 1.0f;
								if (maskStagingTexture2D)
								{
									float mY = (float)y / (float)mipHeight;
									mY *= (float)maskStagingDesc.Height;
									uint8_t* maskRowData = static_cast<uint8_t*>(maskData) + (UINT)mY * maskMappedResource.RowPitch;

									float mX = (float)x / (float)mipWidth;
									mX *= (float)maskStagingDesc.Width;
									uint32_t* maskPixel = reinterpret_cast<uint32_t*>(maskRowData + (UINT)mX * 4);

									maskColor.SetReverse(*maskPixel);
								}
								if (maskColor.a == 0.0f)
									continue;

								DirectX::XMFLOAT3 bary;
								if (!ComputeBarycentric(x + 0.5f, y + 0.5f, p0, p1, p2, bary))
									continue;

								DirectX::XMVECTOR normal = DirectX::XMVectorAdd(DirectX::XMVectorAdd(
									DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n0), bary.x),
									DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n1), bary.y)),
									DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n2), bary.z));

								auto lerp = [](DirectX::XMVECTOR& minValue, DirectX::XMVECTOR& maxValue, float value) -> DirectX::XMVECTOR {
									return DirectX::XMVectorAdd(
										DirectX::XMVectorScale(minValue, 1.0f - value),
										DirectX::XMVectorScale(maxValue, value)
									);
								};

								if (maskColor.r > 0) {
									float strength = (maskColor.r - 0.5f) * 2.0f;
									DirectX::XMVECTOR normalCorrection = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
									if (strength < 0)
										normalCorrection = emptyVector;
									else if (DirectX::XMVectorGetX(normal) < 0)
										normalCorrection = DirectX::XMVectorNegate(normalCorrection);
									normal = lerp(normal, normalCorrection, std::abs(strength));
								}
								if (maskColor.g > 0) {
									float strength = (maskColor.g - 0.5f) * 2.0f;
									DirectX::XMVECTOR normalCorrection = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
									if (strength < 0)
										normalCorrection = emptyVector;
									else if (DirectX::XMVectorGetX(normal) < 0)
										normalCorrection = DirectX::XMVectorNegate(normalCorrection);
									normal = lerp(normal, normalCorrection, std::abs(strength));
								}
								if (maskColor.b > 0) {
									float strength = (maskColor.b - 0.5f) * 2.0f;
									DirectX::XMVECTOR normalCorrection = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
									if (strength < 0)
										normalCorrection = emptyVector;
									else if (DirectX::XMVectorGetX(normal) < 0)
										normalCorrection = DirectX::XMVectorNegate(normalCorrection);
									normal = lerp(normal, normalCorrection, std::abs(strength));
								}
								normal = DirectX::XMVector3Normalize(normal);

								DirectX::XMFLOAT3 normalF3;
								XMStoreFloat3(&normalF3, normal);
								normalF3.x = normalF3.x * 0.5f + 0.5f;
								normalF3.y = normalF3.y * 0.5f + 0.5f;
								normalF3.z = normalF3.z * 0.5f + 0.5f;

								RGBA normalColor(normalF3.x, normalF3.z, normalF3.y);
								RGBA dstColor = RGBA::lerp(srcColor, normalColor, maskColor.a);
								dstColor.a = srcColor.a;
								*srcPixel = dstColor.GetReverse();
							}
						}
					}
					//PerformanceLog(std::string(__func__) + "::" + textureName + "::" + std::to_string(taskIndex), true, false);
				});
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(dstStagingTexture2D.Get(), subresourceIndex);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}));
		}
		for (auto& parallelMip : parallelMips) {
			parallelMip.join();
		}
		concurrency::CurrentScheduler::Detach();

		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
			return nullptr;

		if (maskStagingTexture2D)
		{
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->Unmap(maskStagingTexture2D.Get(), 0);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D;
		hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
		if (FAILED(hr))
		{
			logger::error("{} : Failed to create dst texture ({})", __func__, hr);
			return nullptr;
		}

		// create shader resource view based on output texture
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstShaderResourceView;
		hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstShaderResourceView);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create ShaderResourceView ({})", __func__, hr);
			return nullptr;
		}

		// copy to output texture;
		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		context->CopyResource(dstTexture2D.Get(), dstStagingTexture2D.Get());
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

		RE::NiPointer<RE::NiSourceTexture> output = nullptr;
		bool texCreated = false;
		Shader::TextureLoadManager::GetSingleton().CreateNiTexture(textureName, a_srcTexturePath.empty() ? "None" : a_srcTexturePath, dstTexture2D, dstShaderResourceView, output, texCreated);
		PerformanceLog(std::string(__func__) + "::" + textureName, true, false);
		return output ? output : nullptr;
	}

	bool ObjectNormalMapBaker::ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out)
	{
		DirectX::SimpleMath::Vector2 v0 = { (float)(b.x - a.x), (float)(b.y - a.y) };
		DirectX::SimpleMath::Vector2 v1 = { (float)(c.x - a.x), (float)(c.y - a.y) };
		DirectX::SimpleMath::Vector2 v2 = { px - a.x, py - a.y };

		float d00 = v0.Dot(v0);
		float d01 = v0.Dot(v1);
		float d11 = v1.Dot(v1);
		float d20 = v2.Dot(v0);
		float d21 = v2.Dot(v1);
		float denom = d00 * d11 - d01 * d01;

		if (denom == 0.0f)
			return false;

		float v = (d11 * d20 - d01 * d21) / denom;
		float w = (d00 * d21 - d01 * d20) / denom;
		float u = 1.0f - v - w;

		if (u < 0 || v < 0 || w < 0)
			return false;

		out = { u, v, w };
		return true;
	}
}