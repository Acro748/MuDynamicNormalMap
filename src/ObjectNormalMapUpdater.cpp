#include "ObjectNormalMapUpdater.h"

namespace Mus {
//#define BAKE_TEST1
//#define BAKE_TEST2
//#define BAKE_TEST3

	ObjectNormalMapUpdater::BakeResult ObjectNormalMapUpdater::UpdateObjectNormalMap(TaskID taskID, GeometryData a_data, std::unordered_map<std::size_t, BakeTextureSet> a_bakeSet)
	{
		std::string_view _func_ = __func__;
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID), false, false);
#endif // BAKE_TEST1

		BakeResult result;
		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| a_bakeSet.empty() || a_data.geometries.empty())
		{
			logger::error("{}::{} : Invalid parameters", _func_, taskID.taskID);
			return result;
		}

		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		//is this works?
		/*concurrency::SchedulerPolicy policy = concurrency::CurrentScheduler::GetPolicy();
		policy.SetPolicyValue(concurrency::ContextPriority, THREAD_PRIORITY_LOWEST);
		policy.SetPolicyValue(concurrency::SchedulingProtocol, concurrency::EnhanceScheduleGroupLocality);
		policy.SetPolicyValue(concurrency::DynamicProgressFeedback, true);
		policy.SetPolicyValue(concurrency::TargetOversubscriptionFactor, 1);
		concurrency::CurrentScheduler::Create(policy);*/

		std::future<void> asyncResult;

		//asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync( [&a_data]() { a_data.Subdivision(Config::GetSingleton().GetSubdivision()); });
		//asyncResult.get();
		/*a_data.Subdivision(Config::GetSingleton().GetSubdivision());
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}*/

		asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync( [&a_data]() { a_data.UpdateMap(); });
		asyncResult.get();
		//a_data.UpdateMap();
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		//asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync( [&a_data]() { a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth()); });
		//asyncResult.get();
		/*a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}*/

		asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync([&a_data]() { a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree()); });
		asyncResult.get();
		//a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
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

		std::vector<std::future<void>> parallelBakings;;
		for (auto& bake : a_bakeSet)
		{
			parallelBakings.push_back(bakingThreads->submitAsync([&, bake]() {
				if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
				{
					logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
					return;
				}

				std::size_t bakeIndex = bake.first;

				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcStagingTexture2D, overlayStagingTexture2D, dstStagingTexture2D;
				D3D11_TEXTURE2D_DESC srcStagingDesc = {}, overlayStagingDesc = {}, dstStagingDesc = {}, dstDesc = {};
				D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};

				if (!bake.second.srcTexturePath.empty())
				{
					Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
					D3D11_TEXTURE2D_DESC srcDesc;
					D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
					if (std::string tangentNormalMapPath = GetTangentNormalMapPath(bake.second.srcTexturePath); IsExistFile(tangentNormalMapPath))
					{
						logger::info("{}::{}::{} : {} src texture loading...)", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, tangentNormalMapPath);

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

							srcStagingDesc = srcDesc;
							srcStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
							srcStagingDesc.Usage = D3D11_USAGE_STAGING;
							srcStagingDesc.BindFlags = 0;
							srcStagingDesc.MiscFlags = 0;
							srcStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

							dstShaderResourceViewDesc = srcShaderResourceViewDesc;
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
						logger::info("{}::{}::{} : {} src texture loading...)", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, bake.second.srcTexturePath);

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
							dstShaderResourceViewDesc = srcShaderResourceViewDesc;
						}
					}
					if (!srcTexture2D)
					{
						logger::error("{}::{}::{} : There is no Normalmap! {}", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, bake.second.srcTexturePath);
						return;
					}
				}

				if (!bake.second.overlayTexturePath.empty())
				{
					Microsoft::WRL::ComPtr<ID3D11Texture2D> overlayTexture2D = nullptr;
					logger::info("{}::{}::{} : {} overlay texture loading...)", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, bake.second.overlayTexturePath);
					D3D11_TEXTURE2D_DESC overlayDesc;
					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.overlayTexturePath, overlayDesc, DXGI_FORMAT_R8G8B8A8_UNORM, overlayTexture2D))
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
							logger::error("{}::{}::{} : Failed to create src staging texture ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
							overlayStagingTexture2D = nullptr;
						}
						else
						{
							Shader::ShaderManager::GetSingleton().ShaderContextLock();
							context->CopyResource(overlayStagingTexture2D.Get(), overlayTexture2D.Get());
							Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
							if (FAILED(hr))
							{
								logger::error("{}::{}::{} : Failed to map overlay staging texture ({}|{})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr, bake.second.overlayTexturePath);
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
				dstStagingDesc.MipLevels = 1;
				dstStagingDesc.ArraySize = 1;
				dstStagingDesc.SampleDesc.Count = 1;
				hr = device->CreateTexture2D(&dstStagingDesc, nullptr, &dstStagingTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{}::{} : Failed to create dst staging texture ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
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

				const bool hasSrcData = (srcData != nullptr);
				const bool hasOverlayData = (overlayData != nullptr);

#ifdef BAKE_TEST2
				PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID) + "::" + "Run", false, false);
#endif // BAKE_TEST2

				D3D11_MAPPED_SUBRESOURCE mappedResource;
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(dstStagingTexture2D.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				if (FAILED(hr))
				{
					logger::error("{}::{}::{} Failed to read data from the staging texture ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				const UINT width = dstStagingDesc.Width;
				const UINT height = dstStagingDesc.Height;
				uint8_t* dstData = reinterpret_cast<uint8_t*>(mappedResource.pData);

				const float invWidth = 1.0f / (float)width;
				const float invHeight = 1.0f / (float)height;
				const float srcHeightF = srcData ? (float)srcStagingDesc.Height : 0.0f;
				const float srcWidthF = srcData ? (float)srcStagingDesc.Width : 0.0f;
				const float overlayHeightF = overlayData ? (float)overlayStagingDesc.Height : 0.0f;
				const float overlayWidthF = overlayData ? (float)overlayStagingDesc.Width : 0.0f;

				std::vector<std::future<void>> parallelTris;
				const std::uint32_t subSize = std::max(UINT(1), ((width / 1024) + (height / 1024))) * std::pow(2, Config::GetSingleton().GetDivideTaskQ());
				const std::uint32_t numSubTasks = (totalTaskCount + subSize - 1) / subSize;
				for (std::size_t subIndex = 0; subIndex < subSize; subIndex++)
				{
					const std::uint32_t subOffset = subIndex * numSubTasks;
					const std::uint32_t localTaskCount = std::min(numSubTasks, totalTaskCount - subOffset);
					const std::uint32_t chunkSize = 8;
					const std::uint32_t chunkCount = (localTaskCount + chunkSize - 1) / chunkSize;

					parallelTris.push_back(ThreadPool_TaskModule::GetSingleton().submitAsync([&, subOffset, chunkSize, chunkCount]() {
						concurrency::parallel_for(std::uint32_t(0), chunkCount, [&, subOffset, chunkSize](std::uint32_t taskIndex) {
							std::uint32_t start = taskIndex * chunkSize + subOffset;
							std::uint32_t end = (std::min)(start + chunkSize, totalTaskCount);
							for (std::uint32_t i = start; i < end; i++)
							{
								const std::uint32_t index = a_data.geometries[bakeIndex].second.indicesStart + i * 3;

								const std::uint32_t index0 = a_data.indices[index + 0];
								const std::uint32_t index1 = a_data.indices[index + 1];
								const std::uint32_t index2 = a_data.indices[index + 2];

								const DirectX::XMFLOAT2& u0 = a_data.uvs[index0];
								const DirectX::XMFLOAT2& u1 = a_data.uvs[index1];
								const DirectX::XMFLOAT2& u2 = a_data.uvs[index2];

								const DirectX::XMVECTOR n0v = DirectX::XMLoadFloat3(&a_data.normals[index0]);
								const DirectX::XMVECTOR n1v = DirectX::XMLoadFloat3(&a_data.normals[index1]);
								const DirectX::XMVECTOR n2v = DirectX::XMLoadFloat3(&a_data.normals[index2]);

								const DirectX::XMVECTOR t0v = DirectX::XMLoadFloat3(&a_data.tangents[index0]);
								const DirectX::XMVECTOR t1v = DirectX::XMLoadFloat3(&a_data.tangents[index1]);
								const DirectX::XMVECTOR t2v = DirectX::XMLoadFloat3(&a_data.tangents[index2]);

								const DirectX::XMVECTOR b0v = DirectX::XMLoadFloat3(&a_data.bitangents[index0]);
								const DirectX::XMVECTOR b1v = DirectX::XMLoadFloat3(&a_data.bitangents[index1]);
								const DirectX::XMVECTOR b2v = DirectX::XMLoadFloat3(&a_data.bitangents[index2]);

								//uvToPixel
								const DirectX::XMINT2 p0 = { static_cast<int>(u0.x * width), static_cast<int>(u0.y * height) };
								const DirectX::XMINT2 p1 = { static_cast<int>(u1.x * width), static_cast<int>(u1.y * height) };
								const DirectX::XMINT2 p2 = { static_cast<int>(u2.x * width), static_cast<int>(u2.y * height) };

								const std::int32_t minX = std::max(0, std::min({ p0.x, p1.x, p2.x }));
								const std::int32_t minY = std::max(0, std::min({ p0.y, p1.y, p2.y }));
								const std::int32_t maxX = std::min((std::int32_t)width - 1, std::max({ p0.x, p1.x, p2.x }) + 1);
								const std::int32_t maxY = std::min((std::int32_t)height - 1, std::max({ p0.y, p1.y, p2.y }) + 1);

								const std::int32_t innerMinX = minX;
								const std::int32_t innerMinY = minY;
								const std::int32_t innerMaxX = maxX;
								const std::int32_t innerMaxY = maxY;

								for (std::int32_t y = minY; y < maxY; y++)
								{
									const float mY = (float)y * invHeight;

									uint8_t* srcRowData = nullptr;
									if (hasSrcData)
									{
										const float srcY = mY * srcHeightF;
										srcRowData = srcData + (UINT)srcY * srcMappedResource.RowPitch;
									}

									uint8_t* overlayRowData = nullptr;
									if (hasOverlayData)
									{
										const float overlayY = mY * overlayHeightF;
										overlayRowData = overlayData + (UINT)overlayY * overlayMappedResource.RowPitch;
									}

									std::uint8_t* rowData = dstData + y * mappedResource.RowPitch;
									for (std::int32_t x = minX; x < maxX; x++)
									{
										DirectX::XMFLOAT3 bary;
										if (!ComputeBarycentrics(x, y, p0, p1, p2, bary))
											continue;

										const float mX = (float)x / (float)width;

										RGBA dstColor;
										RGBA overlayColor(1.0f, 1.0f, 1.0f, 0.0f);
										if (hasOverlayData)
										{
											const float overlayX = mX * overlayWidthF;
											const std::uint32_t* overlayPixel = reinterpret_cast<std::uint32_t*>(overlayRowData + (UINT)overlayX * 4);
											overlayColor.SetReverse(*overlayPixel);
										}
										RGBA srcColor(0.5f, 0.5f, 1.0f, 0.0f);
										if (hasSrcData)
										{
											const float srcX = mX * srcWidthF;
											const std::uint32_t* srcPixel = reinterpret_cast<std::uint32_t*>(srcRowData + (UINT)srcX * 4);
											srcColor.SetReverse(*srcPixel);
										}
										if (overlayColor.a < 1.0f)
										{
											const DirectX::XMVECTOR t = DirectX::XMVector3Normalize(
												DirectX::XMVectorAdd(DirectX::XMVectorAdd(
													DirectX::XMVectorScale(t0v, bary.x),
													DirectX::XMVectorScale(t1v, bary.y)),
													DirectX::XMVectorScale(t2v, bary.z)));

											const DirectX::XMVECTOR b = DirectX::XMVector3Normalize(
												DirectX::XMVectorAdd(DirectX::XMVectorAdd(
													DirectX::XMVectorScale(b0v, bary.x),
													DirectX::XMVectorScale(b1v, bary.y)),
													DirectX::XMVectorScale(b2v, bary.z)));

											const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(
												DirectX::XMVectorAdd(DirectX::XMVectorAdd(
													DirectX::XMVectorScale(n0v, bary.x),
													DirectX::XMVectorScale(n1v, bary.y)),
													DirectX::XMVectorScale(n2v, bary.z)));

											const DirectX::XMVECTOR ft = DirectX::XMVector3Normalize(
												DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));

											const DirectX::XMVECTOR cross = DirectX::XMVector3Cross(n, ft);
											const float handedness = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(cross, b)) < 0.0f) ? -1.0f : 1.0f;
											const DirectX::XMVECTOR fb = DirectX::XMVector3Normalize(DirectX::XMVectorScale(cross, handedness));
											const DirectX::XMMATRIX tbn = DirectX::XMMATRIX(ft, fb, n, DirectX::XMVectorSet(0, 0, 0, 1));

											const DirectX::XMVECTOR srcNormalVec = DirectX::XMVectorSet(
												srcColor.r * 2.0f - 1.0f,
												srcColor.g * 2.0f - 1.0f,
												srcColor.b * 2.0f - 1.0f,
												0.0f);

											const DirectX::XMVECTOR detailNormal = DirectX::XMVector3Normalize(
												DirectX::XMVector3TransformNormal(srcNormalVec, tbn));
											const DirectX::XMVECTOR normalResult = DirectX::XMVector3Normalize(
												DirectX::XMVectorLerp(n, detailNormal, srcColor.a));

											const DirectX::XMVECTOR halfVec = DirectX::XMVectorReplicate(0.5f);
											const DirectX::XMVECTOR normalVec = DirectX::XMVectorMultiplyAdd(normalResult, halfVec, halfVec);

											dstColor = RGBA(DirectX::XMVectorGetX(normalVec), DirectX::XMVectorGetZ(normalVec), DirectX::XMVectorGetY(normalVec));
										}
										if (overlayColor.a > 0.0f)
										{
											dstColor = RGBA::lerp(dstColor, overlayColor, overlayColor.a);
										}

										std::uint32_t* dstPixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
										*dstPixel = dstColor.GetReverse() | 0xFF000000;
									}
								}
							}
						});
					}));
				}
				for (auto& parallelTri : parallelTris) {
					parallelTri.get();
				}

				if (!Config::GetSingleton().GetTextureMarginGPU() || Shader::ShaderManager::GetSingleton().IsFailedShader(BleedTextureShaderName.data()))
				{
					ThreadPool_TaskModule::GetSingleton().submitAsync([&]() { BleedTexture(dstData, dstStagingDesc.Width, dstStagingDesc.Height, mappedResource.RowPitch, margin); }).get();
				}

				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(dstStagingTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

#ifdef BAKE_TEST2
				PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID) + "::" + "Run", true, false);
#endif // BAKE_TEST2

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

				if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
				{
					logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
					return;
				}

				Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstShaderResourceView;

				dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstDesc.Usage = D3D11_USAGE_DEFAULT;
				dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
				dstDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
				dstDesc.MipLevels = 0;
				dstDesc.ArraySize = 1;
				dstDesc.CPUAccessFlags = 0;
				dstDesc.SampleDesc.Count = 1;
				hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{} : Failed to create dst texture ({})", _func_, taskID.taskID, hr);
					return;
				}

				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->CopySubresourceRegion(
					dstTexture2D.Get(),
					0,
					0, 0, 0,
					dstStagingTexture2D.Get(),
					0,
					nullptr);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

				dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
				dstShaderResourceViewDesc.Texture2D.MipLevels = -1;
				hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstShaderResourceView);
				if (FAILED(hr)) {
					logger::error("{}::{} : Failed to create ShaderResourceView ({})", _func_, taskID.taskID, hr);
					return;
				}

				if (Config::GetSingleton().GetTextureMarginGPU())
					BleedTextureGPU(taskID, margin, dstShaderResourceView, dstTexture2D);

				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->GenerateMips(dstShaderResourceView.Get());
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
		for (auto& parallelBaking : parallelBakings) {
			parallelBaking.get();
		}
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID), true, false);
#endif // BAKE_TEST1
		return result;
	}

	ObjectNormalMapUpdater::BakeResult ObjectNormalMapUpdater::UpdateObjectNormalMapGPU(TaskID taskID, GeometryData a_data, std::unordered_map<std::size_t, BakeTextureSet> a_bakeSet)
	{
		std::string_view _func_ = __func__;
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID), false, false);
#endif // BAKE_TEST1

		BakeResult result;
		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| a_bakeSet.empty() || a_data.geometries.empty())
		{
			logger::error("{}::{} : Invalid parameters", _func_, taskID.taskID);
			return result;
		}

		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		//is this works?
		/*concurrency::SchedulerPolicy policy = concurrency::CurrentScheduler::GetPolicy();
		policy.SetPolicyValue(concurrency::ContextPriority, THREAD_PRIORITY_LOWEST);
		policy.SetPolicyValue(concurrency::SchedulingProtocol, concurrency::EnhanceScheduleGroupLocality);
		policy.SetPolicyValue(concurrency::DynamicProgressFeedback, true);
		policy.SetPolicyValue(concurrency::TargetOversubscriptionFactor, 1);
		concurrency::CurrentScheduler::Create(policy);*/

		std::future<void> asyncResult;

		//asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync( [&a_data]() { a_data.Subdivision(Config::GetSingleton().GetSubdivision()); });
		//asyncResult.get();
		/*a_data.Subdivision(Config::GetSingleton().GetSubdivision());
		//std::this_thread::yield();
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}*/

		asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync([&a_data]() { a_data.UpdateMap(); });
		asyncResult.get();
		//a_data.UpdateMap();
		//std::this_thread::yield();
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}

		//asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync( [&a_data]() { a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth()); });
		//asyncResult.get();
		/*a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		//std::this_thread::yield();
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
		{
			logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
			return result;
		}*/

		asyncResult = ThreadPool_TaskModule::GetSingleton().submitAsync([&a_data]() { a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree()); });
		asyncResult.get();
		//a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());
		//std::this_thread::yield();
		if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
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

		const std::uint32_t margin = Config::GetSingleton().GetTextureMargin();

		std::vector<std::future<void>> parallelBakings;;
		for (auto& bake : a_bakeSet)
		{
			parallelBakings.push_back(bakingThreads->submitAsync([&, bake]() {
				if (!TaskManager::GetSingleton().IsValidTaskID(taskID))
				{
					logger::error("{}::{} : Invalid taskID", _func_, taskID.taskID);
					return;
				}

				std::size_t bakeIndex = bake.first;

				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D, overlayTexture2D, dstTexture2D;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcShaderResourceView, overlayShaderResourceView, dstShaderResourceView;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstTextureUAV;
				D3D11_TEXTURE2D_DESC dstDesc = {};
				D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};
				D3D11_UNORDERED_ACCESS_VIEW_DESC dstUnorderedViewDesc = {};
				dstUnorderedViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstUnorderedViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				dstUnorderedViewDesc.Texture2D.MipSlice = 0;

				if (!bake.second.srcTexturePath.empty())
				{
					D3D11_TEXTURE2D_DESC srcDesc;
					D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
					if (std::string tangentNormalMapPath = GetTangentNormalMapPath(bake.second.srcTexturePath); IsExistFile(tangentNormalMapPath))
					{
						logger::info("{}::{}::{} : {} src texture loading...)", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, tangentNormalMapPath);

						if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(
							tangentNormalMapPath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN,
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
							dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

							dstShaderResourceViewDesc = srcShaderResourceViewDesc;
							dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
							dstShaderResourceViewDesc.Texture2D.MipLevels = 1;

							hr = device->CreateShaderResourceView(srcTexture2D.Get(), &srcShaderResourceViewDesc, &srcShaderResourceView);
							if (FAILED(hr))
							{
								logger::error("{}::{}::{} : Failed to create src shader resource view ({}|{})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr, tangentNormalMapPath);
								return;
							}
						}
					}
					if (!srcTexture2D)
					{
						logger::error("{}::{}::{} : There is no Normalmap! {}", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, bake.second.srcTexturePath);
						return;
					}
				}

				if (!bake.second.overlayTexturePath.empty())
				{
					logger::info("{}::{}::{} : {} overlay texture loading...)", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, bake.second.overlayTexturePath);
					D3D11_TEXTURE2D_DESC overlayDesc;
					D3D11_SHADER_RESOURCE_VIEW_DESC overlayShaderResourceViewDesc;
					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.overlayTexturePath, overlayDesc, overlayShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM, overlayTexture2D))
					{
						overlayShaderResourceViewDesc.Texture2D.MipLevels = 1;
						hr = device->CreateShaderResourceView(overlayTexture2D.Get(), &overlayShaderResourceViewDesc, &overlayShaderResourceView);
						if (FAILED(hr))
						{
							logger::error("{}::{}::{} : Failed to create overlay shader resource view ({}|{})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr, bake.second.overlayTexturePath);
							return;
						}
					}
				}

				hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{}::{} : Failed to create dst texture 2d ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstShaderResourceView);
				if (FAILED(hr))
				{
					logger::error("{}::{}::{} : Failed to create dst shader resource view ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				hr = device->CreateUnorderedAccessView(dstTexture2D.Get(), &dstUnorderedViewDesc, &dstTextureUAV);
				if (FAILED(hr))
				{
					logger::error("{}::{}::{} : Failed to create dst unordered access view ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				std::vector<uint32_t> triangleIndices;
				std::vector<TileTriangleRange> tileRanges;
				TileInfo tileInfo{
					256,
					dstDesc.Width,
					dstDesc.Height
				};
				GenerateTileTriangleRanges(tileInfo, a_data, a_data.geometries[bakeIndex].second.indicesStart, a_data.geometries[bakeIndex].second.indicesEnd, triangleIndices, tileRanges);

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
					UINT margin;
					UINT padding1;
					UINT padding2;
					UINT padding3;
				};
				ConstBufferData cbData = {};
				cbData.texWidth = dstDesc.Width;
				cbData.texHeight = dstDesc.Height;
				cbData.tileSize = tileInfo.TILE_SIZE;
				cbData.tileCountX = tileInfo.TILE_COUNT_X();
				cbData.tileOffsetX = 0;
				cbData.tileOffsetY = 0;
				cbData.tileIndex = 0;
				cbData.triangleCount = a_data.geometries[bakeIndex].second.indicesCount() / 3;
				D3D11_BUFFER_DESC cbDesc = {};
				cbDesc.ByteWidth = sizeof(ConstBufferData);
				cbDesc.Usage = D3D11_USAGE_DEFAULT;
				cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				D3D11_SUBRESOURCE_DATA cbInitData = {};
				cbInitData.pSysMem = &cbData;
				Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
				hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
				if (FAILED(hr)) {
					logger::error("{}::{}::{} : Failed to create const buffer ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				std::vector<DirectX::XMFLOAT3> vertices(a_data.vertices.begin() + a_data.geometries[bakeIndex].second.vertexStart, a_data.vertices.begin() + a_data.geometries[bakeIndex].second.vertexEnd);
				Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV;
				if (!CreateStructuredBuffer(vertices.data(), UINT(sizeof(DirectX::XMFLOAT3) * vertices.size()), sizeof(DirectX::XMFLOAT3), vertexBuffer, vertexSRV))
					return;

				std::vector<DirectX::XMFLOAT2> uvs(a_data.uvs.begin() + a_data.geometries[bakeIndex].second.uvStart, a_data.uvs.begin() + a_data.geometries[bakeIndex].second.uvEnd);
				Microsoft::WRL::ComPtr<ID3D11Buffer> uvBuffer;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV;
				if (!CreateStructuredBuffer(uvs.data(), UINT(sizeof(DirectX::XMFLOAT2) * uvs.size()), sizeof(DirectX::XMFLOAT2), uvBuffer, uvSRV))
					return;

				std::vector<DirectX::XMFLOAT3> normals(a_data.normals.begin() + a_data.geometries[bakeIndex].second.normalStart, a_data.normals.begin() + a_data.geometries[bakeIndex].second.normalEnd);
				Microsoft::WRL::ComPtr<ID3D11Buffer> normalBuffer;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
				if (!CreateStructuredBuffer(normals.data(), UINT(sizeof(DirectX::XMFLOAT3) * normals.size()), sizeof(DirectX::XMFLOAT3), normalBuffer, normalSRV))
					return;

				std::vector<std::uint32_t> indices(a_data.indices.begin() + a_data.geometries[bakeIndex].second.indicesStart, a_data.indices.begin() + a_data.geometries[bakeIndex].second.indicesEnd);
				Microsoft::WRL::ComPtr<ID3D11Buffer> indicesBuffer;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> indicesSRV;
				if (!CreateStructuredBuffer(indices.data(), UINT(sizeof(std::uint32_t) * indices.size()), sizeof(std::uint32_t), indicesBuffer, indicesSRV))
					return;

				Microsoft::WRL::ComPtr<ID3D11Buffer> tileRangeBuffer;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tileRangeSRV; 
				if (!CreateStructuredBuffer(tileRanges.data(), tileRanges.size() * sizeof(TileTriangleRange), sizeof(TileTriangleRange), tileRangeBuffer, tileRangeSRV))
					return;

				Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState = nullptr;
				D3D11_SAMPLER_DESC samplerDesc = {};
				samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
				samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
				samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
				samplerDesc.MipLODBias = 0.0f;
				samplerDesc.MaxAnisotropy = 4;
				samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
				samplerDesc.BorderColor[0] = 0.0f;
				samplerDesc.BorderColor[1] = 0.0f;
				samplerDesc.BorderColor[2] = 0.0f;
				samplerDesc.BorderColor[3] = 0.0f;
				samplerDesc.MinLOD = 0;
				samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
				hr = device->CreateSamplerState(&samplerDesc, &samplerState);
				if (FAILED(hr))
				{
					logger::error("{}::{}::{} : Failed to create samplerState ({})", _func_, taskID.taskID, a_data.geometries[bakeIndex].first, hr);
					return;
				}

				auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader("UpdateNormalMap");
				if (!shader)
				{
					logger::error("{}::{} : Invalid shader", _func_, taskID.taskID);
					return;
				}

				struct ShaderBackup {
				public:
					void Backup(ID3D11DeviceContext* context) {
						context->CSGetShader(&shader, nullptr, 0);
						context->CSGetConstantBuffers(0, 1, &constBuffer);
						context->CSGetShaderResources(0, 1, &vertexSRV);
						context->CSGetShaderResources(1, 1, &uvSRV);
						context->CSGetShaderResources(2, 1, &normalSRV);
						context->CSGetShaderResources(3, 1, &indicesSRV);
						context->CSGetShaderResources(4, 1, &tileRangeSRV);
						context->CSGetShaderResources(5, 1, &srcSRV);
						context->CSGetShaderResources(6, 1, &overlaySRV);
						context->CSGetUnorderedAccessViews(0, 1, &dstUAV);
						context->CSGetSamplers(0, 1, &samplerState);
					}
					void Revert(ID3D11DeviceContext* context) {
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						context->CSSetShaderResources(0, 1, vertexSRV.GetAddressOf());
						context->CSSetShaderResources(1, 1, uvSRV.GetAddressOf());
						context->CSSetShaderResources(2, 1, normalSRV.GetAddressOf());
						context->CSSetShaderResources(3, 1, indicesSRV.GetAddressOf());
						context->CSSetShaderResources(4, 1, tileRangeSRV.GetAddressOf());
						context->CSSetShaderResources(5, 1, srcSRV.GetAddressOf());
						context->CSSetShaderResources(6, 1, overlaySRV.GetAddressOf());
						context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
						context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
					}
					Shader::ShaderManager::ComputeShader shader;
					Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> indicesSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tileRangeSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> overlaySRV;
					Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstUAV;
					Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
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
						context->CSSetShaderResources(3, 1, indicesSRV.GetAddressOf());
						context->CSSetShaderResources(4, 1, tileRangeSRV.GetAddressOf());
						context->CSSetShaderResources(5, 1, srcShaderResourceView.GetAddressOf());
						context->CSSetShaderResources(6, 1, overlayShaderResourceView.GetAddressOf());
						context->CSSetUnorderedAccessViews(0, 1, dstTextureUAV.GetAddressOf(), nullptr);
						context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
						context->Dispatch(tileInfo.TILE_SIZE / 8, tileInfo.TILE_SIZE / 8, 1);
						shaderBackup.Revert(context);
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					}
				}

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
		for (auto& parallelBaking : parallelBakings) {
			parallelBaking.get();
		}
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + std::to_string(taskID.taskID), true, false);
#endif // BAKE_TEST1
		return result;
	}

	bool ObjectNormalMapUpdater::ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out)
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
	
	bool ObjectNormalMapUpdater::ComputeBarycentrics(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, std::int32_t margin, DirectX::XMFLOAT3& out)
	{
		px += 0.5f;
		py += 0.5f;
		for (float dy = py - margin; dy <= py + margin; dy++) {
			for (float dx = px - margin; dx <= px + margin; dx++) {
				if (ComputeBarycentric(dx, dy, a, b, c, out)) {
					return true;
				}
			}
		}
		return false;
	}
	bool ObjectNormalMapUpdater::ComputeBarycentrics(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out)
	{
		return ComputeBarycentric(px, py, a, b, c, out) || ComputeBarycentric(px + 1, py, a, b, c, out) 
			|| ComputeBarycentric(px, py + 1, a, b, c, out) || ComputeBarycentric(px + 1, py + 1, a, b, c, out);
	}

	void ObjectNormalMapUpdater::GenerateTileTriangleRanges(TileInfo tileInfo, const GeometryData& a_data, const std::size_t indicesStartOffset, const std::size_t indicesEndOffset, std::vector<uint32_t>& outPackedTriangleIndices, std::vector<TileTriangleRange>& outTileRanges)
	{
		std::vector<std::vector<uint32_t>> tileTriangleLists(tileInfo.TILE_COUNT());

		for (size_t i = indicesStartOffset; i < indicesEndOffset; i += 3)
		{
			std::size_t v0 = a_data.indices[i + 0];
			std::size_t v1 = a_data.indices[i + 1];
			std::size_t v2 = a_data.indices[i + 2];
			const DirectX::XMFLOAT2& u0 = a_data.uvs[v0];
			const DirectX::XMFLOAT2& u1 = a_data.uvs[v1];
			const DirectX::XMFLOAT2& u2 = a_data.uvs[v2];

			float minU = (std::min)({ u0.x, u1.x, u2.x });
			float maxU = std::max({ u0.x, u1.x, u2.x });
			float minV = (std::min)({ u0.y, u1.y, u2.y });
			float maxV = std::max({ u0.y, u1.y, u2.y });

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

	bool ObjectNormalMapUpdater::CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut)
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

	std::string ObjectNormalMapUpdater::GetTangentNormalMapPath(std::string a_normalMapPath)
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

	bool ObjectNormalMapUpdater::IsValidPixel(const std::uint32_t a_pixel)
	{
		return (a_pixel & 0xFF000000) != 0;
	}
	bool ObjectNormalMapUpdater::BleedTexture(std::uint8_t* pData, UINT width, UINT height, UINT RowPitch, std::uint32_t margin)
	{
#ifdef BAKE_TEST3
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), false, false);
#endif // BAKE_TEST3

		if (margin <= 0) 
			return true;

		const DirectX::XMINT2 offsets[8] = {
			DirectX::XMINT2(-1, -1), // left up
			DirectX::XMINT2(0, -1), // up
			DirectX::XMINT2(1, -1), // right up
			DirectX::XMINT2(-1,  0), // left
			DirectX::XMINT2(1,  0), // right
			DirectX::XMINT2(-1,  1), // left down
			DirectX::XMINT2(0,  1), // down
			DirectX::XMINT2(1,  1)  // right down
		};

		for (std::uint32_t m = 0; m < margin; m++)
		{
			ThreadPool_TaskModule::GetSingleton().submitAsync([&]() {
				concurrency::concurrent_unordered_map<std::uint32_t*, RGBA> resultColorMap;
				concurrency::parallel_for(UINT(0), height, [&](UINT y) {
					std::uint8_t* rowData = pData + y * RowPitch;
					for (int x = 0; x < width; ++x)
					{
						std::uint32_t* pixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
						if (IsValidPixel(*pixel))
							continue;

						RGBA averageColor(0.0f, 0.0f, 0.0f, 0.0f);
						std::uint8_t validCount = 0;
#pragma unroll(8)
						for (std::uint8_t i = 0; i < 8; i++)
						{
							DirectX::XMINT2 nearCoord = { (INT)x + offsets[i].x, (INT)y + offsets[i].y };
							if (nearCoord.x < 0 || nearCoord.y < 0 ||
								nearCoord.x >= width || nearCoord.y >= height)
								continue;

							std::uint8_t* nearRowData = pData + nearCoord.y * RowPitch;
							std::uint32_t* nearPixel = reinterpret_cast<uint32_t*>(nearRowData + nearCoord.x * 4);
							if (IsValidPixel(*nearPixel))
							{
								RGBA nearColor;
								nearColor.SetReverse(*nearPixel);
								averageColor += nearColor;
								validCount++;
							}
						}

						if (validCount == 0)
							continue;

						RGBA resultColor = averageColor / validCount;
						resultColorMap[pixel] = resultColor;
					}
				});
				for (auto& map : resultColorMap)
				{
					*map.first = map.second.GetReverse();
				}
			});
		}

#ifdef BAKE_TEST3
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), true, false);
#endif // BAKE_TEST3

		return true;
	}

	bool ObjectNormalMapUpdater::BleedTextureGPU(TaskID taskID, std::uint32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
#ifdef BAKE_TEST3
		PerformanceLog(std::string(__func__) + "::" + std::to_string(taskID.taskID), false, false);
#endif // BAKE_TEST3

		if (margin <= 0)
			return true;

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(BleedTextureShaderName.data());
		if (!device || !context || !shader)
			return false;
		
		HRESULT hr;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvInOut->GetDesc(&srvDesc);
		D3D11_TEXTURE2D_DESC desc;
		texInOut->GetDesc(&desc);

		//create buffers
		struct ConstBufferData
		{
			UINT texWidth;
			UINT texHeight;
			INT margin;
			UINT mipLevel;
		};
		static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");

		struct ShaderBackup {
		public:
			void Backup(ID3D11DeviceContext* context) {
				context->CSGetShader(&shader, nullptr, 0);
				context->CSGetConstantBuffers(0, 1, &constBuffer);
				context->CSGetShaderResources(0, 1, &srv);
				context->CSGetUnorderedAccessViews(0, 1, &uav);
			}
			void Revert(ID3D11DeviceContext* context) {
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, srv.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
			}
			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		private:
		} sb;

		const UINT width = desc.Width;
		const UINT height = desc.Height;

		ConstBufferData cbData = {};
		cbData.texWidth = width;
		cbData.texHeight = height;
		cbData.margin = margin;
		cbData.mipLevel = 0;

		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.ByteWidth = sizeof(ConstBufferData);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = 0;
		cbDesc.MiscFlags = 0;
		cbDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA cbInitData = {};
		cbInitData.pSysMem = &cbData;
		cbInitData.SysMemPitch = 0;
		cbInitData.SysMemSlicePitch = 0;
		Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
		hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
		if (FAILED(hr)) {
			logger::error("{}::{} : Failed to create const buffer ({})", __func__, taskID.taskID, hr);
			return false;
		}

		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = 0;
		desc.MipLevels = srvDesc.Texture2D.MipLevels;
		desc.CPUAccessFlags = 0;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
		hr = device->CreateTexture2D(&desc, nullptr, &texture2D);
		if (FAILED(hr)) {
			logger::error("{}::{} : Failed to create const buffer ({})", __func__, taskID.taskID, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		hr = device->CreateUnorderedAccessView(texture2D.Get(), &uavDesc, &uav);
		if (FAILED(hr)) {
			logger::error("{}::{} : Failed to create const buffer ({})", __func__, taskID.taskID, hr);
			return false;
		}

		DirectX::XMUINT2 dispatch = { (UINT)std::ceilf((float)width / 8.0f), (UINT)std::ceilf((float)height / 8.0f) };
		std::uint32_t divideCount = std::pow(2, Config::GetSingleton().GetDivideTaskQ());
		std::uint32_t marginUnit = std::ceilf((float)margin / (float)divideCount);
		for (std::uint32_t dc = 0; dc < divideCount; dc++)
		{
			ThreadPool_TaskModule::GetSingleton().submitAsync([&]() {
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				sb.Backup(context);
				context->CopyResource(texture2D.Get(), texInOut.Get());
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, srvInOut.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
				for (std::uint32_t mu = 0; mu < marginUnit; mu++)
				{
					context->Dispatch(dispatch.x, dispatch.y, 1);
					context->CopyResource(texInOut.Get(), texture2D.Get());
				}
				sb.Revert(context);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}).get();
		}
#ifdef BAKE_TEST3
		PerformanceLog(std::string(__func__) + "::" + std::to_string(taskID.taskID), true, false);
#endif // BAKE_TEST3

		return true;
	}
}