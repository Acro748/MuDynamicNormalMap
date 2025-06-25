#include "ObjectNormalMapBaker.h"

namespace Mus {
#define BAKE_TEST

	ObjectNormalMapBaker::BakeResult ObjectNormalMapBaker::BakeObjectNormalMap(TaskID taskID, GeometryData a_data, std::unordered_map<std::size_t, BakeTextureSet> a_bakeSet)
	{
		std::string_view _func_ = __func__;
#ifdef BAKE_TEST
		PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID), false, false);
#endif // BAKE_TEST

		BakeResult result;
		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| a_bakeSet.empty() || a_data.geometries.empty())
		{
			logger::error("{}::{} : Invalid parameters", _func_, taskID.taskID);
			return result;
		}

		//is this works?
		/*concurrency::SchedulerPolicy policy = concurrency::CurrentScheduler::GetPolicy();
		policy.SetPolicyValue(
			concurrency::ContextPriority, THREAD_PRIORITY_BELOW_NORMAL
		);
		concurrency::CurrentScheduler::Create(policy);*/

		a_data.UpdateMap();
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		a_data.Subdivision(Config::GetSingleton().GetSubdivision());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}
		a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		HRESULT hr;
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
		{
			logger::error("{}::{} : Invalid renderer", _func_, taskID.taskID);
			return result;
		}

		const std::int32_t margin = Config::GetSingleton().GetTextureMargin();

		std::vector<std::thread> bakeThreads;
		for (auto& bake : a_bakeSet)
		{
			bakeThreads.push_back(std::thread([&, bake]() {
				TaskManager::SetDeferredWorker();

				if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
				{
					logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
					return;
				}

				std::size_t bakeIndex = bake.first;

				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcStagingTexture2D, overlayStagingTexture2D, dstStagingTexture2D;
				D3D11_TEXTURE2D_DESC srcStagingDesc = {}, overlayStagingDesc = {}, dstStagingDesc = {}, dstDesc = {};
				D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};
				dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstDesc.Usage = D3D11_USAGE_DEFAULT;
				dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				dstDesc.MiscFlags = 0;
				dstDesc.Width = Config::GetSingleton().GetDefaultTextureWidth();
				dstDesc.Height = Config::GetSingleton().GetDefaultTextureHeight();
				dstDesc.CPUAccessFlags = 0;
				dstDesc.ArraySize = 1;
				dstDesc.MipLevels = 1;
				dstDesc.SampleDesc.Count = 1;
				dstDesc.SampleDesc.Quality = 0;

				dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

				if (!bake.second.srcTexturePath.empty())
				{
					Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
					D3D11_TEXTURE2D_DESC srcDesc;
					D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
					if (std::string tangentNormalMapPath = GetTangentNormalMapPath(bake.second.srcTexturePath); IsExistFile(tangentNormalMapPath))
					{
						logger::info("{}::{} : {} src texture loading...)", _func_, taskID.taskID, tangentNormalMapPath);

						if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(
							tangentNormalMapPath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM,
							Config::GetSingleton().GetIgnoreTextureSize() ? Config::GetSingleton().GetDefaultTextureWidth() : 0,
							Config::GetSingleton().GetIgnoreTextureSize() ? Config::GetSingleton().GetDefaultTextureHeight() : 0,
							srcTexture2D))
						{
							if (!Config::GetSingleton().GetIgnoreTextureSize() && Config::GetSingleton().GetTextureResize() != 1.0f)
							{
								srcDesc.Width = srcDesc.Width * Config::GetSingleton().GetTextureResize();
								srcDesc.Height = srcDesc.Height * Config::GetSingleton().GetTextureResize();
							}

							dstDesc = srcDesc;
							dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

							srcStagingDesc = srcDesc;
							srcStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
							srcStagingDesc.Usage = D3D11_USAGE_STAGING;
							srcStagingDesc.BindFlags = 0;
							srcStagingDesc.MiscFlags = 0;
							srcStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

							dstShaderResourceViewDesc = srcShaderResourceViewDesc;
							dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
							dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

							hr = device->CreateTexture2D(&srcStagingDesc, nullptr, &srcStagingTexture2D);
							if (FAILED(hr))
							{
								logger::error("{}::{} : Failed to create src staging texture ({}|{})", _func_, taskID.taskID, hr, tangentNormalMapPath);
								srcStagingTexture2D = nullptr;
							}

							Shader::ShaderManager::GetSingleton().ShaderContextLock();
							context->CopyResource(srcStagingTexture2D.Get(), srcTexture2D.Get());
							Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
						}
					}
					else
					{
						logger::info("{}::{} : {} src texture loading...)", _func_, taskID.taskID, bake.second.srcTexturePath);

						if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(
							bake.second.srcTexturePath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM,
							Config::GetSingleton().GetIgnoreTextureSize() ? Config::GetSingleton().GetDefaultTextureWidth() : 0,
							Config::GetSingleton().GetIgnoreTextureSize() ? Config::GetSingleton().GetDefaultTextureHeight() : 0,
							srcTexture2D))
						{
							if (!Config::GetSingleton().GetIgnoreTextureSize() && Config::GetSingleton().GetTextureResize() != 1.0f)
							{
								srcDesc.Width = srcDesc.Width * Config::GetSingleton().GetTextureResize();
								srcDesc.Height = srcDesc.Height * Config::GetSingleton().GetTextureResize();
							}

							dstDesc = srcDesc;
							dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

							dstShaderResourceViewDesc = srcShaderResourceViewDesc;
							dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
							dstShaderResourceViewDesc.Texture2D.MipLevels = 1;
						}
					}
					if (!srcTexture2D)
					{
						logger::error("{}::{} : There is no Normalmap! {}", _func_, taskID.taskID, bake.second.srcTexturePath);
						return;
					}
				}

				if (!bake.second.overlayTexturePath.empty())
				{
					Microsoft::WRL::ComPtr<ID3D11Texture2D> overlayTexture2D = nullptr;
					std::string overlayTexturePath = "";
					if (overlayTexturePath = GetOverlayNormalMapPath(bake.second.srcTexturePath); !IsExistFile(overlayTexturePath, ExistType::textures))
						overlayTexturePath = bake.second.overlayTexturePath;
					logger::info("{}::{} : {} overlay texture loading...)", _func_, taskID.taskID, overlayTexturePath);
					D3D11_TEXTURE2D_DESC overlayDesc;
					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(overlayTexturePath, overlayDesc, DXGI_FORMAT_R8G8B8A8_UNORM, overlayTexture2D))
					{
						overlayStagingDesc = overlayDesc;
						overlayStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
						overlayStagingDesc.Usage = D3D11_USAGE_STAGING;
						overlayStagingDesc.BindFlags = 0;
						overlayStagingDesc.MiscFlags = 0;
						overlayStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
						hr = device->CreateTexture2D(&overlayStagingDesc, nullptr, &overlayStagingTexture2D);
						if (FAILED(hr))
						{
							logger::error("{}::{} : Failed to create src staging texture ({})", _func_, taskID.taskID, hr);
							overlayStagingTexture2D = nullptr;
						}
						else
						{
							Shader::ShaderManager::GetSingleton().ShaderContextLock();
							context->CopyResource(overlayStagingTexture2D.Get(), overlayTexture2D.Get());
							Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
							if (FAILED(hr))
							{
								logger::error("{}::{} : Failed to map overlay staging texture ({}|{})", _func_, taskID.taskID, hr, bake.second.overlayTexturePath);
								overlayStagingTexture2D = nullptr;
							}
						}
					}
				}

				dstStagingDesc = dstDesc;
				dstStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstStagingDesc.Usage = D3D11_USAGE_STAGING;
				dstStagingDesc.BindFlags = 0;
				dstStagingDesc.MiscFlags = 0;
				dstStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
				hr = device->CreateTexture2D(&dstStagingDesc, nullptr, &dstStagingTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{} : Failed to create dst staging texture ({})", _func_, taskID.taskID, hr);
					return;
				}

				if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
				{
					logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
					return;
				}

				logger::info("{}::{}::{} : {} {} {} {} baking normalmap...", _func_, taskID.taskID, a_data.geometries[bakeIndex].first,
							 a_data.geometries[bakeIndex].second.vertexCount(),
							 a_data.geometries[bakeIndex].second.uvCount(),
							 a_data.geometries[bakeIndex].second.normalCount(),
							 a_data.geometries[bakeIndex].second.indicesCount());

				std::uint32_t totalTaskCount = a_data.geometries[bakeIndex].second.indicesCount() / 3;

				D3D11_MAPPED_SUBRESOURCE srcMappedResource;
				uint8_t* srcData = nullptr;
				if (srcStagingTexture2D)
				{
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					hr = context->Map(srcStagingTexture2D.Get(), 0, D3D11_MAP_READ, 0, &srcMappedResource);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					srcData = reinterpret_cast<uint8_t*>(srcMappedResource.pData);
				}
				D3D11_MAPPED_SUBRESOURCE overlayMappedResource;
				uint8_t* overlayData = nullptr;
				if (overlayStagingTexture2D)
				{
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					hr = context->Map(overlayStagingTexture2D.Get(), 0, D3D11_MAP_READ, 0, &overlayMappedResource);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					overlayData = reinterpret_cast<uint8_t*>(overlayMappedResource.pData);
				}

				bool isBakeProblem = false;
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
							logger::error("{}::{}::{} Failed to read data from the staging texture ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
							isBakeProblem = true;
							return;
						}

						UINT mipWidth = (std::max)(UINT(1), dstStagingDesc.Width >> mip);
						UINT mipHeight = (std::max)(UINT(1), dstStagingDesc.Height >> mip);
						uint8_t* dstData = reinterpret_cast<uint8_t*>(mappedResource.pData);

						std::uint32_t chunkSize = 16;
						if (mip > 0)
							chunkSize *= std::pow(2, mip + 1);
						std::uint32_t chunkCount = (totalTaskCount + chunkSize - 1) / chunkSize;
						concurrency::parallel_for(std::uint32_t(0), chunkCount, [&](std::uint32_t taskIndex) {
							std::uint32_t start = taskIndex * chunkSize;
							std::uint32_t end = (std::min)(start + chunkSize, totalTaskCount);
							for (std::uint32_t i = start; i < end; i++)
							{
								std::uint32_t index = a_data.geometries[bakeIndex].second.indicesStart + i * 3;

								std::uint32_t index1 = a_data.indices[index + 0];
								std::uint32_t index2 = a_data.indices[index + 1];
								std::uint32_t index3 = a_data.indices[index + 2];

								DirectX::XMFLOAT3& v0 = a_data.vertices[index1];
								DirectX::XMFLOAT3& v1 = a_data.vertices[index2];
								DirectX::XMFLOAT3& v2 = a_data.vertices[index3];

								DirectX::XMFLOAT2& u0 = a_data.uvs[index1];
								DirectX::XMFLOAT2& u1 = a_data.uvs[index2];
								DirectX::XMFLOAT2& u2 = a_data.uvs[index3];

								DirectX::XMFLOAT3& n0 = a_data.normals[index1];
								DirectX::XMFLOAT3& n1 = a_data.normals[index2];
								DirectX::XMFLOAT3& n2 = a_data.normals[index3];

								DirectX::XMFLOAT3& t0 = a_data.tangents[index1];
								DirectX::XMFLOAT3& t1 = a_data.tangents[index2];
								DirectX::XMFLOAT3& t2 = a_data.tangents[index3];

								DirectX::XMFLOAT3& b0 = a_data.bitangents[index1];
								DirectX::XMFLOAT3& b1 = a_data.bitangents[index2];
								DirectX::XMFLOAT3& b2 = a_data.bitangents[index3];

								auto uvToPix = [&](DirectX::XMFLOAT2 uv) -> DirectX::XMINT2 {
									return {
										static_cast<int>(uv.x * mipWidth + 0.5f),
										static_cast<int>(uv.y * mipHeight + 0.5f)
									};
								};

								auto p0 = uvToPix(u0);
								auto p1 = uvToPix(u1);
								auto p2 = uvToPix(u2);

								std::int32_t minX = (std::max)((std::int32_t)0, (std::min)({ p0.x, p1.x, p2.x }) - margin);
								std::int32_t minY = (std::max)((std::int32_t)0, (std::min)({ p0.y, p1.y, p2.y }) - margin);
								std::int32_t maxX = (std::min)((std::int32_t)mipWidth - 1, (std::max)({ p0.x, p1.x, p2.x }) + margin);
								std::int32_t maxY = (std::min)((std::int32_t)mipWidth - 1, (std::max)({ p0.y, p1.y, p2.y }) + margin);

								std::int32_t sizeX = maxX - minX + 1;
								std::int32_t sizeY = maxY - minY + 1;
								std::int32_t sizeXY = sizeX * sizeY;

								for (std::int32_t xy = 0; xy < sizeXY; xy++)
								{
									std::int32_t x = minX + (xy % sizeX);
									std::int32_t y = minY + (xy / sizeX);

									DirectX::XMFLOAT3 bary;
									if (!ComputeBarycentric((float)x + 0.5f, (float)y + 0.5f, p0, p1, p2, bary))
										continue;

									float mY = (float)y / (float)mipHeight;
									float mX = (float)x / (float)mipWidth;

									RGBA dstColor;
									RGBA overlayColor(1.0f, 1.0f, 1.0f, 0.0f);
									if (overlayData)
									{
										float overlayY = mY * (float)overlayStagingDesc.Height;
										uint8_t* overlayRowData = static_cast<uint8_t*>(overlayData) + (UINT)overlayY * overlayMappedResource.RowPitch;
										float overlayX = mX * (float)overlayStagingDesc.Width;
										uint32_t* overlayPixel = reinterpret_cast<uint32_t*>(overlayRowData + (UINT)overlayX * 4);
										overlayColor.SetReverse(*overlayPixel);
									}
									if (overlayColor.a < 1.0f)
									{
										RGBA srcColor(0.5f, 0.5f, 1.0f, 0.5f);
										if (srcData)
										{
											float srcY = mY * (float)srcStagingDesc.Height;
											uint8_t* srcRowData = static_cast<uint8_t*>(srcData) + (UINT)srcY * srcMappedResource.RowPitch;
											float srcX = mX * (float)srcStagingDesc.Width;
											uint32_t* srcPixel = reinterpret_cast<uint32_t*>(srcRowData + (UINT)srcX * 4);
											srcColor.SetReverse(*srcPixel);
										}

										DirectX::XMVECTOR t = DirectX::XMVector3Normalize(
											DirectX::XMVectorAdd(DirectX::XMVectorAdd(
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&t0), bary.x),
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&t1), bary.y)),
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&t2), bary.z)));

										DirectX::XMVECTOR b = DirectX::XMVector3Normalize(
											DirectX::XMVectorAdd(DirectX::XMVectorAdd(
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&b0), bary.x),
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&b1), bary.y)),
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&b2), bary.z)));

										DirectX::XMVECTOR n = DirectX::XMVector3Normalize(
											DirectX::XMVectorAdd(DirectX::XMVectorAdd(
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n0), bary.x),
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n1), bary.y)),
												DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n2), bary.z)));

										DirectX::XMMATRIX tbn = DirectX::XMMATRIX(t, b, n, DirectX::XMVectorSet(0, 0, 0, 1));

										DirectX::XMFLOAT3 detailTangentNormal = {
											srcColor.r * 2.0f - 1.0f,
											srcColor.g * 2.0f - 1.0f,
											srcColor.b * 2.0f - 1.0f,
										};
										DirectX::XMVECTOR detailNormal = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(XMLoadFloat3(&detailTangentNormal), tbn));
										DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVectorLerp(n, detailNormal, srcColor.a));
										DirectX::XMFLOAT3 normalF3;
										DirectX::XMStoreFloat3(&normalF3, normal);
										normalF3.x = normalF3.x * 0.5f + 0.5f;
										normalF3.y = normalF3.y * 0.5f + 0.5f;
										normalF3.z = normalF3.z * 0.5f + 0.5f;

										dstColor = RGBA(normalF3.x, normalF3.z, normalF3.y);
									}
									if (overlayColor.a > 0.0f)
									{
										dstColor = RGBA::lerp(dstColor, overlayColor, overlayColor.a);
										dstColor.a = 1.0f;
									}

									std::uint8_t* rowData = static_cast<std::uint8_t*>(dstData) + y * mappedResource.RowPitch;
									std::uint32_t* dstPixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
									*dstPixel = dstColor.GetReverse();
								}
							}
						});

						std::uint32_t* pixels = reinterpret_cast<uint32_t*>(dstData);
						BleedTexture(pixels, mipWidth, mipHeight, margin);

						Shader::ShaderManager::GetSingleton().ShaderContextLock();
						context->Unmap(dstStagingTexture2D.Get(), subresourceIndex);
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					}));
				}
				for (auto& parallelMip : parallelMips) {
					parallelMip.join();
				}

				if (srcStagingTexture2D)
				{
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					context->Unmap(srcStagingTexture2D.Get(), 0);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				}
				if (overlayStagingTexture2D)
				{
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					context->Unmap(overlayStagingTexture2D.Get(), 0);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				}

				if (isBakeProblem)
				{
					logger::error("{}::{} : Unable to access graphics data", _func_, taskID.taskID);
					return;
				}

				if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
				{
					logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
					return;
				}

				Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D;
				hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{} : Failed to create dst texture ({})", _func_, taskID.taskID, hr);
					return;
				}

				// create shader resource view based on output texture
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstShaderResourceView;
				hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstShaderResourceView);
				if (FAILED(hr)) {
					logger::error("{}::{} : Failed to create ShaderResourceView ({})", _func_, taskID.taskID, hr);
					return;
				}

				// copy to output texture;
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->CopyResource(dstTexture2D.Get(), dstStagingTexture2D.Get());
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

				RE::NiPointer<RE::NiSourceTexture> output = nullptr;
				bool texCreated = false;
				Shader::TextureLoadManager::GetSingleton().CreateNiTexture(a_bakeSet[bakeIndex].textureName, a_bakeSet[bakeIndex].srcTexturePath.empty() ? "None" : a_bakeSet[bakeIndex].srcTexturePath, dstTexture2D, dstShaderResourceView, output, texCreated);
				if (!output)
					return;

				NormalMapResult newNormalMapResult;
				newNormalMapResult.index = bakeIndex;
				newNormalMapResult.vertexCount = a_data.geometries[bakeIndex].second.vertexCount();
				newNormalMapResult.geoName = a_bakeSet[bakeIndex].geometryName;
				newNormalMapResult.textureName = a_bakeSet[bakeIndex].textureName;
				newNormalMapResult.normalmap = output;
				result.push_back(newNormalMapResult);
				logger::info("{}::{}::{} : normalmap baked", _func_, taskID.taskID, a_bakeSet[bakeIndex].geometryName);
			}));
		}
		for (auto& bakeThread : bakeThreads) {
			bakeThread.join();
		}
#ifdef BAKE_TEST
		PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID), true, false);
#endif // BAKE_TEST
		return result;
	}

	RE::NiPointer<RE::NiSourceTexture> ObjectNormalMapBaker::BakeObjectNormalMapGPU(TaskID taskID, std::string textureName, GeometryData a_data, std::string a_srcTexturePath, std::string a_maskTexturePath)
	{
#ifdef BAKE_TEST
		PerformanceLog(std::string(__func__) + "::" + textureName, false, false);
#endif // BAKE_TEST

		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| textureName.empty())
		{
			logger::error("{} : Invalid parameters", __func__);
			return nullptr;
		}

		a_data.UpdateMap();
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{} : Invalid taskID", __func__);
			return nullptr;
		}

		a_data.Subdivision(Config::GetSingleton().GetSubdivision());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{} : Invalid taskID", __func__);
			return nullptr;
		}

		a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{} : Invalid taskID", __func__);
			return nullptr;
		}
		a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());
		if (TaskManager::GetSingleton().GetCurrentBakeObjectNormalMapTaskID(taskID) != taskID.taskID)
		{
			logger::error("{} : Invalid taskID", __func__);
			return nullptr;
		}

		HRESULT hr;
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
		{
			logger::error("{} : Invalid renderer", __func__);
			return nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> maskTexture2D, dstTexture2D;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTextureSRV, dstTextureSRV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstTextureUAV;
		D3D11_TEXTURE2D_DESC dstDesc = {}, maskDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};
		D3D11_UNORDERED_ACCESS_VIEW_DESC dstUnorderedViewDesc = {};
		dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstDesc.Usage = D3D11_USAGE_DEFAULT;
		dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		dstDesc.MiscFlags = 0;
		dstDesc.Width = Config::GetSingleton().GetDefaultTextureWidth();
		dstDesc.Height = Config::GetSingleton().GetDefaultTextureHeight();
		dstDesc.CPUAccessFlags = 0;
		dstDesc.MipLevels = 1;

		dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

		dstUnorderedViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstUnorderedViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		dstUnorderedViewDesc.Texture2D.MipSlice = 0;

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
				dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstDesc.MipLevels = 1;

				dstShaderResourceViewDesc = srcShaderResourceViewDesc;
				dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

				hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
				if (FAILED(hr))
				{
					logger::error("{} : Failed to create dst staging texture ({})", __func__, hr);
					return nullptr;
				}

				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->CopyResource(dstTexture2D.Get(), srcTexture2D.Get());
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
		}

		if (!a_maskTexturePath.empty())
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC maskShaderResourceViewDesc = {};
			if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(a_maskTexturePath, maskDesc, maskShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, maskTexture2D))
			{
				maskShaderResourceViewDesc.Texture2D.MipLevels = 1;

				hr = device->CreateShaderResourceView(maskTexture2D.Get(), &maskShaderResourceViewDesc, &maskTextureSRV);
				if (FAILED(hr))
				{
					logger::error("{} : Failed to create mask shader resource view ({})", __func__, hr);
					return nullptr;
				}
			}
			else
				maskTexture2D = nullptr;
		}

		if (!dstTexture2D)
		{
			dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

			hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
			if (FAILED(hr))
			{
				logger::error("{} : Failed to create dst texture 2d ({})", __func__, hr);
				return nullptr;
			}
		}

		hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstTextureSRV);
		if (FAILED(hr))
		{
			logger::error("{} : Failed to create dst shader resource view ({})", __func__, hr);
			return nullptr;
		}

		hr = device->CreateUnorderedAccessView(dstTexture2D.Get(), &dstUnorderedViewDesc, &dstTextureUAV);
		if (FAILED(hr))
		{
			logger::error("{} : Failed to create dst unordered access view ({})", __func__, hr);
			return nullptr;
		}

		std::vector<uint32_t> triangleIndices;
		std::vector<TileTriangleRange> tileRanges;
		TileInfo tileInfo{
			256,
			dstDesc.Width,
			dstDesc.Height
		};
		//GenerateTileTriangleRanges(tileInfo, a_data, triangleIndices, tileRanges);

		//create buffers
		struct ConstBufferData
		{
			UINT texWidth;
			UINT texHeight;
			UINT tileSize;
			UINT tileCountX;
			UINT tileOffsetX;
			UINT tileOffsetY;
			UINT tileIndex;
			UINT triangleCount;
		};
		ConstBufferData cbData = {};
		cbData.texWidth = dstDesc.Width;
		cbData.texHeight = dstDesc.Height;
		cbData.tileSize = tileInfo.TILE_SIZE;
		cbData.tileCountX = tileInfo.TILE_COUNT_X();
		cbData.tileOffsetX = 0;
		cbData.tileOffsetY = 0;
		cbData.tileIndex = 0;
		cbData.triangleCount = a_data.indices.size() / 3;
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
			return nullptr;
		}

		std::vector<DirectX::XMFLOAT3> vertices(a_data.vertices.begin(), a_data.vertices.end());
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV;
		if (!CreateStructuredBuffer(vertices.data(), UINT(sizeof(DirectX::XMFLOAT3) * vertices.size()), sizeof(DirectX::XMFLOAT3), vertexBuffer, vertexSRV))
			return nullptr;

		std::vector<DirectX::XMFLOAT2> uvs(a_data.uvs.begin(), a_data.uvs.end());
		Microsoft::WRL::ComPtr<ID3D11Buffer> uvBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV;
		if (!CreateStructuredBuffer(uvs.data(), UINT(sizeof(DirectX::XMFLOAT2) * uvs.size()), sizeof(DirectX::XMFLOAT2), uvBuffer, uvSRV))
			return nullptr;

		std::vector<DirectX::XMFLOAT3> normals(a_data.normals.begin(), a_data.normals.end());
		Microsoft::WRL::ComPtr<ID3D11Buffer> normalBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
		if (!CreateStructuredBuffer(normals.data(), UINT(sizeof(DirectX::XMFLOAT3) * normals.size()), sizeof(DirectX::XMFLOAT3), normalBuffer, normalSRV))
			return nullptr;

		std::vector<std::uint32_t> indices(a_data.indices.begin(), a_data.indices.end());
		Microsoft::WRL::ComPtr<ID3D11Buffer> triBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> triSRV;
		if (!CreateStructuredBuffer(indices.data(), UINT(sizeof(std::uint32_t) * indices.size()), sizeof(std::uint32_t) * 3, triBuffer, triSRV))
			return nullptr;

		Microsoft::WRL::ComPtr<ID3D11Buffer> tileBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tileSRV;
		//if (!CreateStructuredBuffer(tileRanges.data(), UINT(sizeof(TileTriangleRange) * tileRanges.size()), sizeof(TileTriangleRange), tileBuffer, tileSRV))
		//	return nullptr;

		auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader("ObjectNormalMapBaker");
		if (!shader)
		{
			logger::error("{} : Invalid shader", __func__);
			return nullptr;
		}

		struct ShaderBackup {
		public:
			void Backup(ID3D11DeviceContext* context) {
				context->CSGetShader(&shader, nullptr, 0);
				context->CSGetConstantBuffers(0, 1, &constBuffer);
				context->CSGetShaderResources(0, 1, &vertexSRV);
				context->CSGetShaderResources(1, 1, &uvSRV);
				context->CSGetShaderResources(2, 1, &normalSRV);
				context->CSGetShaderResources(3, 1, &triSRV);
				context->CSGetShaderResources(4, 1, &tileSRV);
				context->CSGetShaderResources(5, 1, &maskTextureSRV);
				context->CSGetUnorderedAccessViews(0, 1, &dstTextureUAV);
			}
			void Revert(ID3D11DeviceContext* context) {
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, vertexSRV.GetAddressOf());
				context->CSSetShaderResources(1, 1, uvSRV.GetAddressOf());
				context->CSSetShaderResources(2, 1, normalSRV.GetAddressOf());
				context->CSSetShaderResources(3, 1, triSRV.GetAddressOf());
				context->CSSetShaderResources(4, 1, tileSRV.GetAddressOf());
				context->CSSetShaderResources(5, 1, maskTextureSRV.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, dstTextureUAV.GetAddressOf(), nullptr);
			}
			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> triSRV;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tileSRV;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTextureSRV;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstTextureUAV;
		private:
		} shaderBackup;


		for (uint32_t tileY = 0; tileY < tileInfo.TILE_COUNT_Y(); tileY++)
		{
			for (uint32_t tileX = 0; tileX < tileInfo.TILE_COUNT_X(); tileX++)
			{
				ConstBufferData cbTileData = cbData;
				cbTileData.tileOffsetX = tileX * tileInfo.TILE_SIZE;
				cbTileData.tileOffsetY = tileY * tileInfo.TILE_SIZE;
				cbTileData.tileIndex = tileY * tileInfo.TILE_COUNT_X() + tileX;

				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				shaderBackup.Backup(context);
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->UpdateSubresource(constBuffer.Get(), 0, nullptr, &cbTileData, 0, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, vertexSRV.GetAddressOf());
				context->CSSetShaderResources(1, 1, uvSRV.GetAddressOf());
				context->CSSetShaderResources(2, 1, normalSRV.GetAddressOf());
				context->CSSetShaderResources(3, 1, triSRV.GetAddressOf());
				context->CSSetShaderResources(4, 1, tileSRV.GetAddressOf());
				context->CSSetShaderResources(5, 1, maskTextureSRV.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, dstTextureUAV.GetAddressOf(), nullptr);
				context->Dispatch(tileInfo.TILE_SIZE / 8, tileInfo.TILE_SIZE / 8, 1);
				shaderBackup.Revert(context);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
		}

		RE::NiPointer<RE::NiSourceTexture> output = nullptr;
		bool texCreated = false;
		Shader::TextureLoadManager::GetSingleton().CreateNiTexture(textureName, a_srcTexturePath.empty() ? "None" : a_srcTexturePath, dstTexture2D, dstTextureSRV, output, texCreated);
#ifdef BAKE_TEST
		PerformanceLog(std::string(__func__) + "::" + textureName, true, false);
#endif // BAKE_TEST
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

	void ObjectNormalMapBaker::GenerateTileTriangleRanges(TileInfo tileInfo, const GeometryData& a_data, std::vector<uint32_t>& outPackedTriangleIndices, std::vector<TileTriangleRange>& outTileRanges)
	{
		std::vector<std::vector<uint32_t>> tileTriangleLists(tileInfo.TILE_COUNT());

		for (size_t i = 0; i < a_data.indices.size(); i += 3)
		{
			std::size_t v0 = a_data.indices[i + 0];
			std::size_t v1 = a_data.indices[i + 1];
			std::size_t v2 = a_data.indices[i + 2];
			const DirectX::XMFLOAT2& u0 = a_data.uvs[v0];
			const DirectX::XMFLOAT2& u1 = a_data.uvs[v1];
			const DirectX::XMFLOAT2& u2 = a_data.uvs[v2];

			float minU = (std::min)({ u0.x, u1.x, u2.x });
			float maxU = (std::max)({ u0.x, u1.x, u2.x });
			float minV = (std::min)({ u0.y, u1.y, u2.y });
			float maxV = (std::max)({ u0.y, u1.y, u2.y });

			int minX = static_cast<int>(minU * tileInfo.TEX_WIDTH) / tileInfo.TILE_SIZE;
			int maxX = static_cast<int>(maxU * tileInfo.TEX_WIDTH) / tileInfo.TILE_SIZE;
			int minY = static_cast<int>(minV * tileInfo.TEX_HEIGHT) / tileInfo.TILE_SIZE;
			int maxY = static_cast<int>(maxV * tileInfo.TEX_HEIGHT) / tileInfo.TILE_SIZE;

			minX = std::clamp(minX, 0, int(tileInfo.TILE_COUNT_X() - 1));
			maxX = std::clamp(maxX, 0, int(tileInfo.TILE_COUNT_X() - 1));
			minY = std::clamp(minY, 0, int(tileInfo.TILE_COUNT_Y() - 1));
			maxY = std::clamp(maxY, 0, int(tileInfo.TILE_COUNT_Y() - 1));

			for (int ty = minY; ty <= maxY; ++ty)
			{
				for (int tx = minX; tx <= maxX; ++tx)
				{
					int tileIndex = ty * tileInfo.TILE_COUNT_X() + tx;
					tileTriangleLists[tileIndex].push_back((uint32_t)i / 3);
				}
			}
		}

		outPackedTriangleIndices.clear();
		outTileRanges.resize(tileInfo.TILE_COUNT());

		for (uint32_t tileIndex = 0; tileIndex < tileInfo.TILE_COUNT(); ++tileIndex)
		{
			const auto& list = tileTriangleLists[tileIndex];
			outTileRanges[tileIndex].startOffset = (uint32_t)outPackedTriangleIndices.size();
			outTileRanges[tileIndex].count = (uint32_t)list.size();

			for (uint32_t triIndex : list)
			{
				outPackedTriangleIndices.push_back(triIndex);
			}
		}
	}

	bool ObjectNormalMapBaker::CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = size;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = stride;
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = data;

		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		auto hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateBuffer(&desc, &initData, &buffer);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create buffer ({})", __func__, hr);
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = size / stride;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateShaderResourceView(buffer.Get(), &srvDesc, &srv);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create buffer shader resource view ({})", __func__, hr);
			return false;
		}

		bufferOut = buffer;
		srvOut = srv;
		return true;
	};

	std::string ObjectNormalMapBaker::GetTangentNormalMapPath(std::string a_normalMapPath)
	{
		constexpr std::string_view prefix = "Textures\\";
		constexpr std::string_view msn_suffix = "_msn";
		constexpr std::string_view n_suffix = "_n";
		if (a_normalMapPath.empty())
			return "";
		if (!stringStartsWith(a_normalMapPath, prefix.data()))
			a_normalMapPath = prefix.data() + a_normalMapPath;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		if (stringEndsWith(filename, msn_suffix.data())) //_msn -> _n
		{
			filename = stringRemoveEnds(filename, msn_suffix.data());
			filename += n_suffix;
			return (file.parent_path() / (filename + ".dds")).string();
		}
		else if (stringEndsWith(filename, n_suffix.data())) //_n
			return a_normalMapPath;
		return "";
	}
	std::string ObjectNormalMapBaker::GetOverlayNormalMapPath(std::string a_normalMapPath)
	{
		constexpr std::string_view prefix = "Textures\\";
		constexpr std::string_view msn_suffix = "_msn";
		constexpr std::string_view n_suffix = "_n";
		constexpr std::string_view ov_suffix = "_ov";
		if (a_normalMapPath.empty())
			return "";
		if (!stringStartsWith(a_normalMapPath, prefix.data()))
			a_normalMapPath = prefix.data() + a_normalMapPath;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();

		if (stringEndsWith(filename, msn_suffix.data())) //_msn -> _n_ov
		{
			filename = stringRemoveEnds(filename, msn_suffix.data());
			filename += n_suffix;
			filename += ov_suffix;
			return (file.parent_path() / (filename + ".dds")).string();
		}
		else if (stringEndsWith(filename, n_suffix.data())) //_n -> _n_ov
		{
			filename += ov_suffix;
			return (file.parent_path() / (filename + ".dds")).string();
		}
		else if (stringEndsWith(filename, ov_suffix.data())) //_ov
			return a_normalMapPath;
		return "";
	}
	bool ObjectNormalMapBaker::IsInvalidPixel(const std::uint32_t a_pixel)
	{
		RGBA pixel;
		pixel.SetReverse(a_pixel);
		return pixel.a == 0;
	}
	void ObjectNormalMapBaker::BleedTexture(std::uint32_t* pixels, UINT width, UINT height, std::int32_t margin)
	{
#ifdef BAKE_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), false, false);
#endif // BAKE_TEST

		if (margin == 0)
			return;

		struct PixelCoord {
			int x, y;
		};
		std::vector<std::uint32_t> original(pixels, pixels + width * height);
		std::vector<std::uint8_t> visited(width* height, 0);
		concurrency::concurrent_queue<PixelCoord> frontier;

		auto GetIndex = [&](int x, int y) {
			return y * width + x;
		};

		concurrency::parallel_for(UINT(0), height, [&](UINT y) {
			for (int x = 0; x < width; ++x) {
				int idx = GetIndex(x, y);
				if (IsInvalidPixel(original[idx]))
					continue;

				static const int offsets[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
				for (auto& off : offsets) {
					int nx = x + off[0];
					int ny = y + off[1];
					if (nx < 0 || ny < 0 || nx >= width || ny >= height)
						continue;

					int nidx = GetIndex(nx, ny);
					if (IsInvalidPixel(original[nidx]) && !visited[nidx]) {
						visited[nidx] = 1;
						pixels[nidx] = original[idx];
						frontier.push({ nx, ny });
					}
				}
			}
		});

		for (int step = 1; step < margin; ++step) {
			std::vector<PixelCoord> currentFrontier;
			PixelCoord p;
			while (frontier.try_pop(p)) {
				currentFrontier.push_back(p);
			}

			concurrency::parallel_for(std::size_t(0), currentFrontier.size(), [&](std::size_t i) {
				int x = currentFrontier[i].x;
				int y = currentFrontier[i].y;
				int idx = GetIndex(x, y);
				std::uint32_t srcPixel = pixels[idx];

				static const int offsets[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
				for (auto& off : offsets) {
					int nx = x + off[0];
					int ny = y + off[1];
					if (nx < 0 || ny < 0 || nx >= width || ny >= height)
						continue;

					int nidx = GetIndex(nx, ny);
					if (IsInvalidPixel(pixels[nidx]) && !visited[nidx]) {
						if (_InterlockedExchange8(reinterpret_cast<volatile char*>(&visited[nidx]), 1) == 0) {
							pixels[nidx] = srcPixel;
							frontier.push({ nx, ny });
						}
					}
				}
			});
		}

#ifdef BAKE_TEST
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), true, false);
#endif // BAKE_TEST
	}
}