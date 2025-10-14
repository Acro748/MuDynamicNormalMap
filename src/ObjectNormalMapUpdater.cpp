#include "ObjectNormalMapUpdater.h"

namespace Mus {
	void ObjectNormalMapUpdater::onEvent(const FrameEvent& e)
	{
		if (ResourceDataMap.empty())
			return;

		static std::clock_t lastTickTime = currentTime;
		if (lastTickTime + 100 > currentTime) //0.1sec
			return;
		lastTickTime = currentTime;
		memoryManageThreads->submitAsync([&]() {
			ResourceDataMapLock.lock();
			auto ResourceDataMap_ = ResourceDataMap;
			ResourceDataMap.clear();
			ResourceDataMapLock.unlock();
			for (const auto& map : ResourceDataMap_)
			{
				if (map->time < 0)
				{
					if (map->IsQueryDone())
						map->time = currentTime;
				}
				else if (map->time + Config::GetSingleton().GetWaitForRendererTickMS() < currentTime)
				{
					logger::debug("{} : Removed garbage texture resource", map->textureName);
					continue;
				}

				ResourceDataMapLock.lock_shared();
				ResourceDataMap.push_back(map);
				ResourceDataMapLock.unlock_shared();
			}
			if (ResourceDataMap.size() > 0)
				logger::debug("Current remain texture resource {}", ResourceDataMap.size());
		});
	}

	void ObjectNormalMapUpdater::onEvent(const PlayerCellChangeEvent& e)
	{
		if (!e.IsChangedInOut)
			return;
		memoryManageThreads->submitAsync([&]() {
			ResourceDataMapLock.lock_shared();
			auto GeometryResourceDataMap_ = GeometryResourceDataMap;
			ResourceDataMapLock.unlock_shared();
			for (const auto& map : GeometryResourceDataMap_)
			{
				RE::Actor* actor = GetFormByID<RE::Actor*>(map.first);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					ResourceDataMapLock.lock();
					GeometryResourceDataMap.unsafe_erase(map.first);
					ResourceDataMapLock.unlock();
					logger::debug("{:x} : Removed garbage geometry resource", map.first);
				}
			}
			ResourceDataMapLock.lock_shared();
			if (GeometryResourceDataMap.size() > 0)
				logger::debug("Current remain geometry resource {}", GeometryResourceDataMap.size());
			ResourceDataMapLock.unlock_shared();
		});
	}

	void ObjectNormalMapUpdater::Init()
	{
		if (Config::GetSingleton().GetGPUEnable())
		{
			if (!samplerState)
			{
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
				const auto device = GetDevice();
				if (!device)
				{
					logger::error("{} : Invalid device", __func__);
					return;
				}
				auto hr = device->CreateSamplerState(&samplerDesc, &samplerState);
				if (FAILED(hr))
				{
					logger::error("{} : Failed to create samplerState ({})", __func__, hr);
					return;
				}
			}
			Shader::ShaderManager::GetSingleton().GetComputeShader(GetDevice(), UpdateNormalMapShaderName.data());
		}
		if (Config::GetSingleton().GetTextureMarginGPU())
		{
			Shader::ShaderManager::GetSingleton().GetComputeShader(GetDevice(), BleedTextureShaderName.data());
		}
		if (Config::GetSingleton().GetMergeTextureGPU())
		{
			Shader::ShaderManager::GetSingleton().GetComputeShader(GetDevice(), MergeTextureShaderName.data());
		}
	}

	bool ObjectNormalMapUpdater::CreateGeometryResourceData(RE::FormID a_actorID, GeometryDataPtr a_data)
	{
		a_data->GetGeometryData();

		if (a_data->vertices.empty() || a_data->indices.empty()
			|| a_data->vertices.size() != a_data->uvs.size()
			|| a_data->geometries.empty() || a_actorID == 0)
		{
			logger::error("{} : Invalid parameters", __func__);
			return false;
		}
		a_data->Subdivision(Config::GetSingleton().GetSubdivision(), Config::GetSingleton().GetSubdivisionTriThreshold());
		a_data->UpdateMap();
		a_data->VertexSmoothByAngle(Config::GetSingleton().GetVertexSmoothByAngleThreshold1(), Config::GetSingleton().GetVertexSmoothByAngleThreshold2(), Config::GetSingleton().GetVertexSmoothByAngle());
		a_data->VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		a_data->RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());

		if (a_data->vertices.size() != a_data->uvs.size() ||
			a_data->vertices.size() != a_data->normals.size() ||
			a_data->vertices.size() != a_data->tangents.size() ||
			a_data->vertices.size() != a_data->bitangents.size())
		{
			logger::error("{} : Invalid geometry", __func__);
			return false;
		}

		if (Config::GetSingleton().GetGPUEnable())
		{
			const auto device = GetDevice();
			if (!device)
			{
				logger::error("{} : Invalid device", __func__);
				return false;
			}
			GeometryResourceDataPtr newGeometryResourceData = std::make_shared<GeometryResourceData>();
			if (!CreateStructuredBuffer(device, a_data->vertices.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->vertices.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->vertexBuffer, newGeometryResourceData->vertexSRV))
				return false;
			if (!CreateStructuredBuffer(device, a_data->uvs.data(), UINT(sizeof(DirectX::XMFLOAT2) * a_data->uvs.size()), sizeof(DirectX::XMFLOAT2), newGeometryResourceData->uvBuffer, newGeometryResourceData->uvSRV))
				return false;
			if (!CreateStructuredBuffer(device, a_data->normals.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->normals.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->normalBuffer, newGeometryResourceData->normalSRV))
				return false;
			if (!CreateStructuredBuffer(device, a_data->tangents.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->tangents.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->tangentBuffer, newGeometryResourceData->tangentSRV))
				return false;
			if (!CreateStructuredBuffer(device, a_data->bitangents.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->bitangents.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->bitangentBuffer, newGeometryResourceData->bitangentSRV))
				return false;
			if (!CreateStructuredBuffer(device, a_data->indices.data(), UINT(sizeof(std::uint32_t) * a_data->indices.size()), sizeof(std::uint32_t), newGeometryResourceData->indicesBuffer, newGeometryResourceData->indicesSRV))
				return false;

			ResourceDataMapLock.lock_shared();
			{
				auto found = GeometryResourceDataMap.find(a_actorID);
				if (found == GeometryResourceDataMap.end())
				{
					GeometryResourceDataMap.insert(std::make_pair(a_actorID, newGeometryResourceData));
				}
				else
				{
					found->second = newGeometryResourceData;
				}
			}
			ResourceDataMapLock.unlock_shared();
		}
		return true;
	}
	void ObjectNormalMapUpdater::ClearGeometryResourceData()
	{
		ResourceDataMapLock.lock();
		GeometryResourceDataMap.clear();
		ResourceDataMapLock.unlock();
	}

	ObjectNormalMapUpdater::GeometryResourceDataPtr ObjectNormalMapUpdater::GetGeometryResourceData(RE::FormID a_actorID)
	{
		ResourceDataMapLock.lock_shared();
		auto found = GeometryResourceDataMap.find(a_actorID);
		GeometryResourceDataPtr results = found != GeometryResourceDataMap.end() ? found->second : nullptr;
		ResourceDataMapLock.unlock_shared();
		return results;
	}

	std::uint64_t ObjectNormalMapUpdater::GetHash(UpdateTextureSet updateSet, std::uint64_t geoHash)
	{
		return std::hash<std::string>()(std::to_string(updateSet.slot) + "|"
										+ updateSet.srcTexturePath + "|"
										+ updateSet.detailTexturePath + "|"
										+ updateSet.overlayTexturePath + "|"
										+ updateSet.maskTexturePath + "|"
										+ std::to_string(updateSet.detailStrength) + "|"
										+ std::to_string(geoHash));
	}

	ObjectNormalMapUpdater::UpdateResult ObjectNormalMapUpdater::UpdateObjectNormalMap(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet)
	{
		WaitForFreeVram();

		const std::string_view _func_ = __func__;

		UpdateResult results;

		HRESULT hr;
		const auto device = GetDevice();
		const auto context = GetContext();
		if (!device || !context)
		{
			logger::error("{}::{:x} : Invalid renderer", _func_, a_actorID);
			return results;
		}

		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_updateSet.size());
		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(GetContext());
		const auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		const auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		std::unordered_set<RE::BSGeometry*> mergedTextureGeometries;
		concurrency::concurrent_vector<TextureResourceDataPtr> resourceDatas;
		{
			auto updateSet_ = a_updateSet;
			for (const auto& update : updateSet_) {
				const auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
					return geosInfo.geometry == update.first;
												});
				if (found == a_data->geometries.end())
				{
					logger::error("{}::{:x} : Geometry {} not found in data", _func_, a_actorID, update.second.geometryName);
					continue;
				}

				found->hash = GetHash(update.second, found->hash);

				if (auto textureResource = NormalMapStore::GetSingleton().GetResource(found->hash); textureResource)
				{
					logger::info("{}::{:x}::{} : Found exist resource", _func_, a_actorID, update.second.geometryName);

					NormalMapResult newNormalMapResult;
					newNormalMapResult.slot = update.second.slot;
					newNormalMapResult.geometry = update.first;
					newNormalMapResult.vertexCount = found->objInfo.vertexCount();
					newNormalMapResult.geoName = update.second.geometryName;
					newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
					newNormalMapResult.textureName = update.second.textureName;
					newNormalMapResult.texture = std::make_shared<TextureResource>(*textureResource);
					newNormalMapResult.hash = found->hash;
					newNormalMapResult.existResource = true;
					results.push_back(newNormalMapResult);

					TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
					newResourceData->textureName = update.second.textureName;
					resourceDatas.push_back(newResourceData);

					a_updateSet.unsafe_erase(update.first);
				}
			}

			updateSet_ = a_updateSet;
			for (const auto& update : updateSet_) {
				const auto pairResultFound = std::find_if(results.begin(), results.end(), [&](const NormalMapResult& results) {
					return update.first != results.geometry && update.second.textureName == results.textureName;
														  });
				if (pairResultFound == results.end())
					continue;

				logger::info("{}::{:x}::{} : Found exist resource", _func_, a_actorID, update.second.geometryName);

				results.push_back(*pairResultFound);

				TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
				newResourceData->textureName = update.second.textureName;
				resourceDatas.push_back(newResourceData);

				a_updateSet.unsafe_erase(update.first);

				mergedTextureGeometries.insert(update.first);
			}
		}

		std::mutex resultLock;
		for (const auto& update : a_updateSet)
		{
			if (Config::GetSingleton().GetUpdateNormalMapTime1())
				PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, false, false);

			auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
				return geosInfo.geometry == update.first;
			});
			if (found == a_data->geometries.end())
			{
				logger::error("{}::{:x} : Geometry {} not found in data", _func_, a_actorID, update.second.geometryName);
				continue;
			}
			GeometryData::ObjectInfo& objInfo = found->objInfo;
			TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
			newResourceData->textureName = update.second.textureName;

			D3D11_TEXTURE2D_DESC srcStagingDesc = {}, detailStagingDesc = {}, overlayStagingDesc = {}, maskStagingDesc = {}, dstStagingDesc = {}, dstDesc = {};
			D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};

			if (!update.second.srcTexturePath.empty())
			{
				if (!IsDetailNormalMap(update.second.srcTexturePath))
				{
					logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.srcTexturePath);

					if (LoadTextureCPU(device, context, update.second.srcTexturePath, srcStagingDesc, dstShaderResourceViewDesc, newResourceData->srcTexture2D))
					{
						dstDesc = srcStagingDesc;
						dstDesc.Width = Config::GetSingleton().GetTextureWidth();
						dstDesc.Height = Config::GetSingleton().GetTextureHeight();
					}
				}
			}
			if (!update.second.detailTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} detail texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.detailTexturePath);

				if (LoadTextureCPU(device, context, update.second.detailTexturePath, detailStagingDesc, dstShaderResourceViewDesc, newResourceData->detailTexture2D))
				{
					dstDesc = detailStagingDesc;
					dstDesc.Width = std::max(dstDesc.Width, Config::GetSingleton().GetTextureWidth());
					dstDesc.Height = std::max(dstDesc.Height, Config::GetSingleton().GetTextureHeight());
				}
			}
			if (!newResourceData->srcTexture2D && !newResourceData->detailTexture2D)
			{
				logger::error("{}::{:x}::{} : There is no Normalmap!", _func_, a_actorID, update.second.geometryName);
				continue;
			}

			if (!update.second.overlayTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} overlay texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.overlayTexturePath);
				LoadTextureCPU(device, context, update.second.overlayTexturePath, overlayStagingDesc, newResourceData->overlayTexture2D);
			}

			if (!update.second.maskTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} mask texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.maskTexturePath);
				LoadTextureCPU(device, context, update.second.maskTexturePath, maskStagingDesc, newResourceData->maskTexture2D);
			}
			WaitForGPU(device, context).Wait();

			dstStagingDesc = dstDesc;
			dstStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstStagingDesc.Usage = D3D11_USAGE_STAGING;
			dstStagingDesc.BindFlags = 0;
			dstStagingDesc.MiscFlags = 0;
			dstStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			dstStagingDesc.MipLevels = 1;
			dstStagingDesc.ArraySize = 1;
			dstStagingDesc.SampleDesc.Count = 1;
			hr = device->CreateTexture2D(&dstStagingDesc, nullptr, &newResourceData->dstWriteTexture2D);
			if (FAILED(hr))
			{
				logger::error("{}::{:x}::{} : Failed to create dst staging texture ({})", _func_, a_actorID, update.second.geometryName, hr);
				continue;
			}

			logger::info("{}::{:x}::{} : {} {} {} {} baking normalmap...", _func_, a_actorID, update.second.geometryName,
						 objInfo.vertexCount(),
						 objInfo.uvCount(),
						 objInfo.normalCount(),
						 objInfo.indicesCount());

			D3D11_MAPPED_SUBRESOURCE mappedResource, srcMappedResource, detailMappedResource, overlayMappedResource, maskMappedResource;
			std::uint8_t* srcData = nullptr;
			std::uint8_t* detailData = nullptr;
			std::uint8_t* overlayData = nullptr;
			std::uint8_t* maskData = nullptr;

			ShaderLock();
			hr = context->Map(newResourceData->dstWriteTexture2D.Get(), 0, D3D11_MAP_WRITE, 0, &mappedResource);
			if (newResourceData->srcTexture2D)
			{
				hr = context->Map(newResourceData->srcTexture2D.Get(), 0, D3D11_MAP_READ, 0, &srcMappedResource);
				srcData = reinterpret_cast<std::uint8_t*>(srcMappedResource.pData);
			}
			if (newResourceData->detailTexture2D)
			{
				hr = context->Map(newResourceData->detailTexture2D.Get(), 0, D3D11_MAP_READ, 0, &detailMappedResource);
				detailData = reinterpret_cast<std::uint8_t*>(detailMappedResource.pData);
			}
			if (newResourceData->overlayTexture2D)
			{
				hr = context->Map(newResourceData->overlayTexture2D.Get(), 0, D3D11_MAP_READ, 0, &overlayMappedResource);
				overlayData = reinterpret_cast<std::uint8_t*>(overlayMappedResource.pData);
			}
			if (newResourceData->maskTexture2D)
			{
				hr = context->Map(newResourceData->maskTexture2D.Get(), 0, D3D11_MAP_READ, 0, &maskMappedResource);
				maskData = reinterpret_cast<std::uint8_t*>(maskMappedResource.pData);
			}
			ShaderUnlock();

			const bool hasSrcData = (srcData != nullptr);
			const bool hasDetailData = (detailData != nullptr);
			const bool hasOverlayData = (overlayData != nullptr);
			const bool hasMaskData = (maskData != nullptr);

			if (Config::GetSingleton().GetUpdateNormalMapTime1())
				PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, true, false);

			std::uint32_t totalTris = objInfo.indicesCount() / 3;

			const UINT width = dstStagingDesc.Width;
			const UINT height = dstStagingDesc.Height;
			uint8_t* dstData = reinterpret_cast<uint8_t*>(mappedResource.pData);

			const float WidthF = (float)width;
			const float HeightF = (float)height;
			const float invWidth = 1.0f / WidthF;
			const float invHeight = 1.0f / HeightF;
			const float srcWidthF = hasSrcData ? (float)srcStagingDesc.Width : 0.0f;
			const float srcHeightF = hasSrcData ? (float)srcStagingDesc.Height : 0.0f;
			const float detailWidthF = hasDetailData ? (float)detailStagingDesc.Width : 0.0f;
			const float detailHeightF = hasDetailData ? (float)detailStagingDesc.Height : 0.0f;
			const float overlayWidthF = hasOverlayData ? (float)overlayStagingDesc.Width : 0.0f;
			const float overlayHeightF = hasOverlayData ? (float)overlayStagingDesc.Height : 0.0f;
			const float maskWidthF = hasMaskData ? (float)maskStagingDesc.Width : 0.0f;
			const float maskHeightF = hasMaskData ? (float)maskStagingDesc.Height : 0.0f;

			const DirectX::XMVECTOR halfVec = DirectX::XMVectorReplicate(0.5f);
			const bool tangentZCorrection = Config::GetSingleton().GetTangentZCorrection();
			const float detailStrength = update.second.detailStrength;

			std::vector<std::future<void>> processes;
			std::size_t sub = std::min((std::size_t)totalTris, processingThreads->GetThreads() * 16);
			std::size_t unit = (totalTris + sub - 1) / sub;

			if (Config::GetSingleton().GetUpdateNormalMapTime2())
				PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, false, false);

			for (std::size_t p = 0; p < sub; p++) {
				std::size_t begin = p * unit;
				std::size_t end = std::min(begin + unit, (std::size_t)totalTris);
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++) {
						const std::uint32_t index = objInfo.indicesStart + i * 3;

						const std::uint32_t index0 = a_data->indices[index + 0];
						const std::uint32_t index1 = a_data->indices[index + 1];
						const std::uint32_t index2 = a_data->indices[index + 2];

						const DirectX::XMFLOAT2& u0 = a_data->uvs[index0];
						const DirectX::XMFLOAT2& u1 = a_data->uvs[index1];
						const DirectX::XMFLOAT2& u2 = a_data->uvs[index2];

						const DirectX::XMVECTOR n0v = DirectX::XMLoadFloat3(&a_data->normals[index0]);
						const DirectX::XMVECTOR n1v = DirectX::XMLoadFloat3(&a_data->normals[index1]);
						const DirectX::XMVECTOR n2v = DirectX::XMLoadFloat3(&a_data->normals[index2]);

						const DirectX::XMVECTOR t0v = DirectX::XMLoadFloat3(&a_data->tangents[index0]);
						const DirectX::XMVECTOR t1v = DirectX::XMLoadFloat3(&a_data->tangents[index1]);
						const DirectX::XMVECTOR t2v = DirectX::XMLoadFloat3(&a_data->tangents[index2]);

						const DirectX::XMVECTOR b0v = DirectX::XMLoadFloat3(&a_data->bitangents[index0]);
						const DirectX::XMVECTOR b1v = DirectX::XMLoadFloat3(&a_data->bitangents[index1]);
						const DirectX::XMVECTOR b2v = DirectX::XMLoadFloat3(&a_data->bitangents[index2]);

						//uvToPixel
						const DirectX::XMINT2 p0 = { static_cast<int>(u0.x * width), static_cast<int>(u0.y * height) };
						const DirectX::XMINT2 p1 = { static_cast<int>(u1.x * width), static_cast<int>(u1.y * height) };
						const DirectX::XMINT2 p2 = { static_cast<int>(u2.x * width), static_cast<int>(u2.y * height) };

						const std::int32_t minX = std::max(0, std::min({ p0.x, p1.x, p2.x }));
						const std::int32_t minY = std::max(0, std::min({ p0.y, p1.y, p2.y }));
						const std::int32_t maxX = std::min((std::int32_t)width - 1, std::max({ p0.x, p1.x, p2.x }) + 1);
						const std::int32_t maxY = std::min((std::int32_t)height - 1, std::max({ p0.y, p1.y, p2.y }) + 1);

						for (std::int32_t y = minY; y < maxY; y++)
						{
							const float mY = (float)y * invHeight;

							uint8_t* srcRowData = nullptr;
							if (hasSrcData)
							{
								const float srcY = mY * srcHeightF;
								srcRowData = srcData + (UINT)srcY * srcMappedResource.RowPitch;
							}

							uint8_t* detailRowData = nullptr;
							if (hasDetailData)
							{
								const float detailY = mY * detailHeightF;
								detailRowData = detailData + (UINT)detailY * detailMappedResource.RowPitch;
							}

							uint8_t* overlayRowData = nullptr;
							if (hasOverlayData)
							{
								const float overlayY = mY * overlayHeightF;
								overlayRowData = overlayData + (UINT)overlayY * overlayMappedResource.RowPitch;
							}

							uint8_t* maskRowData = nullptr;
							if (hasMaskData)
							{
								const float maskY = mY * maskHeightF;
								maskRowData = maskData + (UINT)maskY * maskMappedResource.RowPitch;
							}

							std::uint8_t* rowData = dstData + y * mappedResource.RowPitch;
							for (std::int32_t x = minX; x < maxX; x++)
							{
								DirectX::XMFLOAT3 bary;
								if (!ComputeBarycentric((float)x + 0.5f, (float)y + 0.5f, p0, p1, p2, bary))
									continue;

								const float mX = x * invWidth;

								RGBA dstColor;
								RGBA overlayColor(1.0f, 1.0f, 1.0f, 0.0f);
								if (hasOverlayData)
								{
									const float overlayX = mX * overlayWidthF;
									const std::uint32_t* overlayPixel = reinterpret_cast<std::uint32_t*>(overlayRowData + (UINT)overlayX * 4);
									overlayColor.SetReverse(*overlayPixel);
								}
								if (overlayColor.a < 1.0f)
								{
									RGBA maskColor(1.0f, 1.0f, 1.0f, 0.0f);
									if (hasMaskData && hasSrcData)
									{
										const float maskX = mX * maskWidthF;
										const std::uint32_t* maskPixel = reinterpret_cast<std::uint32_t*>(maskRowData + (UINT)maskX * 4);
										maskColor.SetReverse(*maskPixel);
									}
									if (maskColor.a < 1.0f)
									{
										RGBA detailColor(0.5f, 0.5f, 1.0f, 0.5f);
										if (hasDetailData)
										{
											const float detailX = mX * detailWidthF;
											const std::uint32_t* detailPixel = reinterpret_cast<std::uint32_t*>(detailRowData + (UINT)detailX * 4);
											detailColor.SetReverse(*detailPixel);
											detailColor = RGBA::lerp(RGBA(0.5f, 0.5f, 1.0f, detailColor.a), detailColor, detailStrength);
										}

										const float denomal = (bary.x + bary.y + floatPrecision);
										const DirectX::XMVECTOR n01 = SlerpVector(n0v, n1v, bary.y / denomal);
										const DirectX::XMVECTOR n = SlerpVector(n01, n2v, bary.z);

										DirectX::XMVECTOR normalResult = emptyVector;
										if (detailColor.a > 0.0f)
										{
											const DirectX::XMVECTOR t01 = SlerpVector(t0v, t1v, bary.y / denomal);
											const DirectX::XMVECTOR t = SlerpVector(t01, t2v, bary.z);

											const DirectX::XMVECTOR b01 = SlerpVector(b0v, b1v, bary.y / denomal);
											const DirectX::XMVECTOR b = SlerpVector(b01, b2v, bary.z);

											const DirectX::XMVECTOR ft = DirectX::XMVector3Normalize(
												DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));
											const DirectX::XMVECTOR fb = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(n, ft));

											const DirectX::XMMATRIX tbn = DirectX::XMMATRIX(ft, fb, n, DirectX::XMVectorSet(0, 0, 0, 1));

											const DirectX::XMFLOAT4 detailColorF(
												detailColor.r * 2.0f - 1.0f,
												detailColor.g * 2.0f - 1.0f,
												detailColor.b * 2.0f - 1.0f,
												0.0f
											);
											const DirectX::XMVECTOR detailNormalVec = DirectX::XMVectorSet(
												detailColorF.x,
												detailColorF.y,
												tangentZCorrection ? std::sqrt(std::max(0.0f, 1.0f - detailColorF.x * detailColorF.x - detailColorF.y * detailColorF.y)) : detailColorF.z,
												0.0f);

											const DirectX::XMVECTOR detailNormal = DirectX::XMVector3Normalize(
												DirectX::XMVector3TransformNormal(detailNormalVec, tbn));
											normalResult = DirectX::XMVector3Normalize(
												DirectX::XMVectorLerp(n, detailNormal, detailColor.a));
										}
										else
										{
											normalResult = n;
										}
										const DirectX::XMVECTOR normalVec = DirectX::XMVectorMultiplyAdd(normalResult, halfVec, halfVec);
										dstColor = RGBA(DirectX::XMVectorGetX(normalVec), DirectX::XMVectorGetZ(normalVec), DirectX::XMVectorGetY(normalVec));
									}
									if (maskColor.a > 0.0f && hasSrcData)
									{
										const float srcX = mX * srcWidthF;
										const std::uint32_t* srcPixel = reinterpret_cast<std::uint32_t*>(srcRowData + (UINT)srcX * 4);
										RGBA srcColor;
										srcColor.SetReverse(*srcPixel);
										dstColor = RGBA::lerp(dstColor, srcColor, maskColor.a);
									}
								}
								if (overlayColor.a > 0.0f)
								{
									dstColor = RGBA::lerp(dstColor, overlayColor, overlayColor.a);
								}

								std::uint32_t* dstPixel = reinterpret_cast<std::uint32_t*>(rowData + x * 4);
								*dstPixel = dstColor.GetReverse() | 0xFF000000;
							}
						}
					}
				}));
			}
			for (auto& process : processes)
			{
				process.get();
			}
			if (Config::GetSingleton().GetUpdateNormalMapTime2())
				PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, true, false);

			ShaderLock();
			context->Unmap(newResourceData->dstWriteTexture2D.Get(), 0);
			if (hasSrcData)
				context->Unmap(newResourceData->srcTexture2D.Get(), 0);
			if (hasDetailData)
				context->Unmap(newResourceData->detailTexture2D.Get(), 0);
			if (hasOverlayData)
				context->Unmap(newResourceData->overlayTexture2D.Get(), 0);
			if (hasMaskData)
				context->Unmap(newResourceData->maskTexture2D.Get(), 0);
			ShaderUnlock();

			WaitForGPU(device, context).Wait();

			dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstDesc.Usage = D3D11_USAGE_DEFAULT;
			dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (Config::GetSingleton().GetUseMipMap() ? D3D11_BIND_RENDER_TARGET : 0);
			dstDesc.MiscFlags = Config::GetSingleton().GetUseMipMap() ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
			dstDesc.MipLevels = Config::GetSingleton().GetUseMipMap() ? 0 : 1;
			dstDesc.ArraySize = 1;
			dstDesc.CPUAccessFlags = 0;
			dstDesc.SampleDesc.Count = 1;
			hr = device->CreateTexture2D(&dstDesc, nullptr, &newResourceData->dstTexture2D);
			if (FAILED(hr))
			{
				logger::error("{}::{:x} : Failed to create dst texture ({})", _func_, a_actorID, hr);
				continue;
			}

			CopySubresourceRegion(device, context, newResourceData->dstTexture2D.Get(), newResourceData->dstWriteTexture2D.Get(), 0, 0);

			dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
			dstShaderResourceViewDesc.Texture2D.MipLevels = Config::GetSingleton().GetUseMipMap() ? -1 : 1;
			hr = device->CreateShaderResourceView(newResourceData->dstTexture2D.Get(), &dstShaderResourceViewDesc, &newResourceData->dstShaderResourceView);
			if (FAILED(hr)) {
				logger::error("{}::{:x} : Failed to create ShaderResourceView ({})", _func_, a_actorID, hr);
				continue;
			}

			NormalMapResult newNormalMapResult;
			newNormalMapResult.slot = update.second.slot;
			newNormalMapResult.geometry = update.first;
			newNormalMapResult.vertexCount = objInfo.vertexCount();
			newNormalMapResult.geoName = update.second.geometryName;
			newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
			newNormalMapResult.textureName = update.second.textureName;
			newNormalMapResult.texture = std::make_shared<TextureResource>();
			newNormalMapResult.texture->normalmapTexture2D = newResourceData->dstTexture2D;
			newNormalMapResult.texture->normalmapShaderResourceView = newResourceData->dstShaderResourceView;
			newNormalMapResult.hash = found->hash;

			resultLock.lock();
			results.push_back(newNormalMapResult);
			resourceDatas.push_back(newResourceData);
			resultLock.unlock();
		}
		WaitForGPU(device, context).Wait();

		if (MergeTexture(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		if (GenerateMipMap(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		if (BleedTexture(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		if (CompressTexture(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		CopyResourceToMain(device, context, resourceDatas, results, mergedTextureGeometries);
		return results;
	}

	ObjectNormalMapUpdater::UpdateResult ObjectNormalMapUpdater::UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet)
	{
		WaitForFreeVram();

		const std::string_view _func_ = __func__;

		UpdateResult results;
		if (!samplerState)
		{
			logger::error("{}::{:x} : Invalid SampleState", _func_, a_actorID);
			return results;
		}

		HRESULT hr;
		const auto device = GetDevice();
		const auto context = GetContext();
		if (!device || !context)
		{
			logger::error("{}::{:x} : Invalid renderer", _func_, a_actorID);
			return results;
		}

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(GetContext());
		const auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		const auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		const GeometryResourceDataPtr geoData = GetGeometryResourceData(a_actorID);
		if (!geoData)
		{
			logger::error("{}::{:x} : GeometryResourceData not found for actor", _func_, a_actorID);
			return results;
		}
		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_updateSet.size());

		std::unordered_set<RE::BSGeometry*> mergedTextureGeometries;
		concurrency::concurrent_vector<TextureResourceDataPtr> resourceDatas;
		{
			auto updateSet_ = a_updateSet;
			for (const auto& update : updateSet_) {
				const auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
					return geosInfo.geometry == update.first;
												});
				if (found == a_data->geometries.end())
				{
					logger::error("{}::{:x} : Geometry {} not found in data", _func_, a_actorID, update.second.geometryName);
					continue;
				}

				found->hash = GetHash(update.second, found->hash);

				if (auto textureResource = NormalMapStore::GetSingleton().GetResource(found->hash); textureResource)
				{
					logger::info("{}::{:x}::{} : Found exist resource", _func_, a_actorID, update.second.geometryName);

					NormalMapResult newNormalMapResult;
					newNormalMapResult.slot = update.second.slot;
					newNormalMapResult.geometry = update.first;
					newNormalMapResult.vertexCount = found->objInfo.vertexCount();
					newNormalMapResult.geoName = update.second.geometryName;
					newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
					newNormalMapResult.textureName = update.second.textureName;
					newNormalMapResult.texture = std::make_shared<TextureResource>(*textureResource);
					newNormalMapResult.hash = found->hash;
					newNormalMapResult.existResource = true;
					results.push_back(newNormalMapResult);

					TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
					newResourceData->textureName = update.second.textureName;
					resourceDatas.push_back(newResourceData);

					a_updateSet.unsafe_erase(update.first);
				}
			}

			updateSet_ = a_updateSet;
			for (const auto& update : updateSet_) {
				const auto pairResultFound = std::find_if(results.begin(), results.end(), [&](const NormalMapResult& results) {
					return update.first != results.geometry && update.second.textureName == results.textureName;
				});
				if (pairResultFound == results.end())
					continue;

				logger::info("{}::{:x}::{} : Found exist resource", _func_, a_actorID, update.second.geometryName);

				results.push_back(*pairResultFound);

				TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
				newResourceData->textureName = update.second.textureName;
				resourceDatas.push_back(newResourceData);

				a_updateSet.unsafe_erase(update.first);

				mergedTextureGeometries.insert(update.first);
			}
		}

		std::mutex resultLock;
		std::vector<std::future<void>> updateTasks;
		for (const auto& update : a_updateSet)
		{
			auto updateTaskFunc = [&, update]() {
				if (Config::GetSingleton().GetUpdateNormalMapTime1())
					PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, false, false);

				const auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
					return geosInfo.geometry == update.first;
				});
				if (found == a_data->geometries.end())
				{
					logger::error("{}::{:x} : Geometry {} not found in data", _func_, a_actorID, update.second.geometryName);
					return;
				}
				const GeometryData::ObjectInfo& objInfo = found->objInfo;
				TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
				newResourceData->textureName = update.second.textureName;

				D3D11_TEXTURE2D_DESC srcDesc = {}, detailDesc = {}, overlayDesc = {}, maskDesc = {}, dstDesc = {}, dstWriteDesc = {};
				D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};

				if (!update.second.srcTexturePath.empty())
				{
					if (!IsDetailNormalMap(update.second.srcTexturePath))
					{
						logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.srcTexturePath);
						if (LoadTexture(device, context, update.second.srcTexturePath, srcDesc, dstShaderResourceViewDesc, newResourceData->srcTexture2D, newResourceData->srcShaderResourceView))
						{
							dstDesc = srcDesc;
							dstDesc.Width = Config::GetSingleton().GetTextureWidth();
							dstDesc.Height = Config::GetSingleton().GetTextureHeight();
						}
					}
				}
				if (!update.second.detailTexturePath.empty())
				{
					logger::info("{}::{:x}::{} : {} detail texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.detailTexturePath);
					if (LoadTexture(device, context, update.second.detailTexturePath, detailDesc, dstShaderResourceViewDesc, newResourceData->detailTexture2D, newResourceData->detailShaderResourceView))
					{
						dstDesc = detailDesc;
						dstDesc.Width = std::max(dstDesc.Width, Config::GetSingleton().GetTextureWidth());
						dstDesc.Height = std::max(dstDesc.Height, Config::GetSingleton().GetTextureHeight());
					}
				}
				if (!newResourceData->srcShaderResourceView && !newResourceData->detailShaderResourceView)
				{
					logger::error("{}::{:x}::{} : There is no Normalmap!", _func_, a_actorID, update.second.geometryName);
					return;
				}

				if (!update.second.overlayTexturePath.empty())
				{
					logger::info("{}::{:x}::{} : {} overlay texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.overlayTexturePath);
					LoadTexture(device, context, update.second.overlayTexturePath, overlayDesc, newResourceData->overlayTexture2D, newResourceData->overlayShaderResourceView);
				}

				if (!update.second.maskTexturePath.empty())
				{
					logger::info("{}::{:x}::{} : {} mask texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.maskTexturePath);
					LoadTexture(device, context, update.second.maskTexturePath, maskDesc, newResourceData->maskTexture2D, newResourceData->maskShaderResourceView);
				}

				dstWriteDesc = dstDesc;
				dstWriteDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstWriteDesc.Usage = D3D11_USAGE_DEFAULT;
				dstWriteDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				dstWriteDesc.MiscFlags = 0;
				dstWriteDesc.MipLevels = 1;
				dstWriteDesc.CPUAccessFlags = 0;
				hr = device->CreateTexture2D(&dstWriteDesc, nullptr, &newResourceData->dstWriteTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{:x}::{} : Failed to create dst texture 2d ({})", _func_, a_actorID, update.second.geometryName, hr);
					return;
				}

				D3D11_UNORDERED_ACCESS_VIEW_DESC dstUnorderedViewDesc = {};
				dstUnorderedViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstUnorderedViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				dstUnorderedViewDesc.Texture2D.MipSlice = 0;
				hr = device->CreateUnorderedAccessView(newResourceData->dstWriteTexture2D.Get(), &dstUnorderedViewDesc, &newResourceData->dstWriteTextureUAV);
				if (FAILED(hr))
				{
					logger::error("{}::{:x}::{} : Failed to create dst unordered access view ({})", _func_, a_actorID, update.second.geometryName, hr);
					return;
				}

				const UINT width = dstDesc.Width;
				const UINT height = dstDesc.Height;

				//create buffers
				struct ConstBufferData
				{
					UINT texWidth;
					UINT texHeight;
					UINT indicesStart;
					UINT indicesEnd;

					UINT hasSrcTexture;
					UINT hasDetailTexture;
					UINT hasOverlayTexture;
					UINT hasMaskTexture;

					UINT tangentZCorrection;
					float detailStrength;
					UINT padding1;
					UINT padding2;
				};
				static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");
				ConstBufferData cbData = {};
				cbData.texWidth = dstDesc.Width;
				cbData.texHeight = dstDesc.Height;
				cbData.indicesStart = objInfo.indicesStart;
				cbData.indicesEnd = objInfo.indicesEnd;
				cbData.hasSrcTexture = newResourceData->srcShaderResourceView ? 1 : 0;
				cbData.hasDetailTexture = newResourceData->detailShaderResourceView ? 1 : 0;
				cbData.hasOverlayTexture = newResourceData->overlayShaderResourceView ? 1 : 0;
				cbData.hasMaskTexture = newResourceData->maskShaderResourceView ? 1 : 0;
				cbData.tangentZCorrection = Config::GetSingleton().GetTangentZCorrection() ? 1 : 0;
				cbData.detailStrength = update.second.detailStrength;

				D3D11_BUFFER_DESC cbDesc = {};
				cbDesc.ByteWidth = sizeof(ConstBufferData);
				cbDesc.Usage = D3D11_USAGE_DEFAULT;
				cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

				if (Config::GetSingleton().GetUpdateNormalMapTime1())
					PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, true, false);

				const auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(device, UpdateNormalMapShaderName.data());
				if (!shader)
				{
					logger::error("{}::{:x} : Invalid shader", _func_, a_actorID);
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
						context->CSGetShaderResources(3, 1, &tangentSRV);
						context->CSGetShaderResources(4, 1, &bitangentSRV);
						context->CSGetShaderResources(5, 1, &indicesSRV);
						context->CSGetShaderResources(6, 1, &srcSRV);
						context->CSGetShaderResources(7, 1, &detailSRV);
						context->CSGetShaderResources(8, 1, &overlaySRV);
						context->CSGetShaderResources(9, 1, &maskSRV);
						context->CSGetUnorderedAccessViews(0, 1, &dstUAV);
						context->CSGetSamplers(0, 1, &samplerState);
						Unbind(context);
					}
					void Revert(ID3D11DeviceContext* context) {
						Unbind(context);
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						context->CSSetShaderResources(0, 1, vertexSRV.GetAddressOf());
						context->CSSetShaderResources(1, 1, uvSRV.GetAddressOf());
						context->CSSetShaderResources(2, 1, normalSRV.GetAddressOf());
						context->CSSetShaderResources(3, 1, tangentSRV.GetAddressOf());
						context->CSSetShaderResources(4, 1, bitangentSRV.GetAddressOf());
						context->CSSetShaderResources(5, 1, indicesSRV.GetAddressOf());
						context->CSSetShaderResources(6, 1, srcSRV.GetAddressOf());
						context->CSSetShaderResources(7, 1, detailSRV.GetAddressOf());
						context->CSSetShaderResources(8, 1, overlaySRV.GetAddressOf());
						context->CSSetShaderResources(9, 1, maskSRV.GetAddressOf());
						context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
						context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
					}
				private:
					void Unbind(ID3D11DeviceContext* context) {
						ID3D11Buffer* emptyBuffer[1] = { nullptr };
						ID3D11ShaderResourceView* emptySRV[10] = { nullptr };
						ID3D11UnorderedAccessView* emptyUAV[1] = { nullptr };
						ID3D11SamplerState* emptySamplerState[1] = { nullptr };

						context->CSSetShader(nullptr, nullptr, 0);
						context->CSSetConstantBuffers(0, 1, emptyBuffer);
						context->CSSetShaderResources(0, 10, emptySRV);
						context->CSSetUnorderedAccessViews(0, 1, emptyUAV, nullptr);
						context->CSSetSamplers(0, 1, emptySamplerState);
					}

					Shader::ShaderManager::ComputeShader shader;
					Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tangentSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bitangentSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> indicesSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> detailSRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> overlaySRV;
					Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskSRV;
					Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstUAV;
					Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
				};

				const std::uint32_t totalTris = objInfo.indicesCount() / 3;
				if (isSecondGPU || isNoSplitGPU)
				{
					const std::uint32_t dispatch = (totalTris + 64 - 1) / 64;
					D3D11_SUBRESOURCE_DATA cbInitData = {};
					cbInitData.pSysMem = &cbData;
					Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
					hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
					if (FAILED(hr)) {
						logger::error("{}::{:x}::{} : Failed to create const buffer ({})", _func_, a_actorID, update.second.geometryName, hr);
						return;
					}

					if (Config::GetSingleton().GetUpdateNormalMapTime2())
						GPUPerformanceLog(device, context, std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, false, false);

					ShaderBackup sb;
					ShaderLock();
					sb.Backup(context);
					context->CSSetShader(shader.Get(), nullptr, 0);
					context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
					context->CSSetShaderResources(0, 1, geoData->vertexSRV.GetAddressOf());
					context->CSSetShaderResources(1, 1, geoData->uvSRV.GetAddressOf());
					context->CSSetShaderResources(2, 1, geoData->normalSRV.GetAddressOf());
					context->CSSetShaderResources(3, 1, geoData->tangentSRV.GetAddressOf());
					context->CSSetShaderResources(4, 1, geoData->bitangentSRV.GetAddressOf());
					context->CSSetShaderResources(5, 1, geoData->indicesSRV.GetAddressOf());
					context->CSSetShaderResources(6, 1, newResourceData->srcShaderResourceView.GetAddressOf());
					context->CSSetShaderResources(7, 1, newResourceData->detailShaderResourceView.GetAddressOf());
					context->CSSetShaderResources(8, 1, newResourceData->overlayShaderResourceView.GetAddressOf());
					context->CSSetShaderResources(9, 1, newResourceData->maskShaderResourceView.GetAddressOf());
					context->CSSetUnorderedAccessViews(0, 1, newResourceData->dstWriteTextureUAV.GetAddressOf(), nullptr);
					context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
					context->Dispatch(dispatch, 1, 1);
					sb.Revert(context);
					ShaderUnlock();

					if (Config::GetSingleton().GetUpdateNormalMapTime2())
						GPUPerformanceLog(device, context, std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, true, false);

					newResourceData->constBuffers.push_back(constBuffer);
				}
				else
				{
					const std::uint32_t subSize = std::max(1u, totalTris / 10000);

					const std::uint32_t numSubTris = (totalTris + subSize - 1) / subSize;
					const std::uint32_t dispatch = (numSubTris + 64 - 1) / 64;
					std::vector<std::future<void>> gpuTasks;
					for (std::size_t subIndex = 0; subIndex < subSize; subIndex++)
					{
						const std::uint32_t trisStart = objInfo.indicesStart + subIndex * numSubTris * 3;
						auto cbData_ = cbData;
						cbData_.indicesStart = trisStart;
						D3D11_SUBRESOURCE_DATA cbInitData = {};
						cbInitData.pSysMem = &cbData_;
						Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
						hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
						if (FAILED(hr)) {
							logger::error("{}::{:x}::{} : Failed to create const buffer ({})", _func_, a_actorID, update.second.geometryName, hr);
							return;
						}
						gpuTasks.push_back(gpuTask->submitAsync([&, constBuffer, subIndex]() {
							if (Config::GetSingleton().GetUpdateNormalMapTime2())
								GPUPerformanceLog(device, context, std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName + "::" + std::to_string(subIndex), false, false);

							ShaderBackup sb;
							ShaderLock();
							sb.Backup(context);
							context->CSSetShader(shader.Get(), nullptr, 0);
							context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
							context->CSSetShaderResources(0, 1, geoData->vertexSRV.GetAddressOf());
							context->CSSetShaderResources(1, 1, geoData->uvSRV.GetAddressOf());
							context->CSSetShaderResources(2, 1, geoData->normalSRV.GetAddressOf());
							context->CSSetShaderResources(3, 1, geoData->tangentSRV.GetAddressOf());
							context->CSSetShaderResources(4, 1, geoData->bitangentSRV.GetAddressOf());
							context->CSSetShaderResources(5, 1, geoData->indicesSRV.GetAddressOf());
							context->CSSetShaderResources(6, 1, newResourceData->srcShaderResourceView.GetAddressOf());
							context->CSSetShaderResources(7, 1, newResourceData->detailShaderResourceView.GetAddressOf());
							context->CSSetShaderResources(8, 1, newResourceData->overlayShaderResourceView.GetAddressOf());
							context->CSSetShaderResources(9, 1, newResourceData->maskShaderResourceView.GetAddressOf());
							context->CSSetUnorderedAccessViews(0, 1, newResourceData->dstWriteTextureUAV.GetAddressOf(), nullptr);
							context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
							context->Dispatch(dispatch, 1, 1);
							sb.Revert(context);
							ShaderUnlock();

							if (Config::GetSingleton().GetUpdateNormalMapTime2())
								GPUPerformanceLog(device, context, std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName + "::" + std::to_string(subIndex), true, false);
						}));
						newResourceData->constBuffers.push_back(constBuffer);
					}
					for (auto& task : gpuTasks) {
						task.get();
					}
				}

				dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstDesc.Usage = D3D11_USAGE_DEFAULT;
				dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (Config::GetSingleton().GetUseMipMap() ? D3D11_BIND_RENDER_TARGET : 0);
				dstDesc.MiscFlags = Config::GetSingleton().GetUseMipMap() ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
				dstDesc.MipLevels = Config::GetSingleton().GetUseMipMap() ? 0 : 1;
				dstDesc.ArraySize = 1;
				dstDesc.CPUAccessFlags = 0;
				dstDesc.SampleDesc.Count = 1;
				hr = device->CreateTexture2D(&dstDesc, nullptr, &newResourceData->dstTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{:x} : Failed to create dst texture ({})", _func_, a_actorID, hr);
					return;
				}

				dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
				dstShaderResourceViewDesc.Texture2D.MipLevels = Config::GetSingleton().GetUseMipMap() ? -1 : 1;
				hr = device->CreateShaderResourceView(newResourceData->dstTexture2D.Get(), &dstShaderResourceViewDesc, &newResourceData->dstShaderResourceView);
				if (FAILED(hr)) {
					logger::error("{}::{:x} : Failed to create ShaderResourceView ({})", _func_, a_actorID, hr);
					return;
				}

				NormalMapResult newNormalMapResult;
				newNormalMapResult.slot = update.second.slot;
				newNormalMapResult.geometry = update.first;
				newNormalMapResult.vertexCount = objInfo.vertexCount();
				newNormalMapResult.geoName = update.second.geometryName;
				newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
				newNormalMapResult.textureName = update.second.textureName;
				newNormalMapResult.texture = std::make_shared<TextureResource>();
				newNormalMapResult.texture->normalmapTexture2D = newResourceData->dstTexture2D;
				newNormalMapResult.texture->normalmapShaderResourceView = newResourceData->dstShaderResourceView;
				newNormalMapResult.hash = found->hash;

				resultLock.lock();
				results.push_back(newNormalMapResult);
				resourceDatas.push_back(newResourceData);
				resultLock.unlock();
			};
			updateTasks.push_back(updateThreads->submitAsync([updateTaskFunc] {
				updateTaskFunc();
			}));
		}
		for (auto& updateTask : updateTasks) {
			updateTask.get();
		}
		WaitForGPU(device, context).Wait();

		for (std::uint32_t i = 0; i < results.size(); i++) {
			if (results[i].existResource)
				continue;
			CopySubresourceRegion(device, context, resourceDatas[i]->dstTexture2D.Get(), resourceDatas[i]->dstWriteTexture2D.Get(), 0, 0);
		}
		WaitForGPU(device, context).Wait();

		if (MergeTexture(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		if (GenerateMipMap(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		if (BleedTexture(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		if (CompressTexture(device, context, resourceDatas, results, mergedTextureGeometries))
			WaitForGPU(device, context).Wait();

		CopyResourceToMain(device, context, resourceDatas, results, mergedTextureGeometries);
		return results;
	}

	bool ObjectNormalMapUpdater::IsDetailNormalMap(const std::string& a_normalMapPath)
	{
		constexpr std::string_view n_suffix = "_n";
		if (a_normalMapPath.empty())
			return false;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		return stringEndsWith(filename, n_suffix.data());
	}

	bool ObjectNormalMapUpdater::LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texOutput, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOutput)
	{
		if (!device || !context || filePath.empty())
			return false;

		HRESULT hr;
		D3D11_TEXTURE2D_DESC tmpTexDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC tmpSrvDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		if (Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context))
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTextureFromFile(device, context, filePath, tmpTexDesc, tmpSrvDesc, DXGI_FORMAT_UNKNOWN, texture))
				return false;
		}
		else
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTexture2D(filePath, tmpTexDesc, tmpSrvDesc, DXGI_FORMAT_UNKNOWN, texture))
				return false;
		}

		tmpSrvDesc.Texture2D.MipLevels = 1;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		hr = device->CreateShaderResourceView(texture.Get(), &tmpSrvDesc, &srv);
		if (FAILED(hr))
		{
			logger::error("Failed to create shader resource view ({}|{})", hr, filePath);
			return false;
		}
		texDesc = tmpTexDesc;
		srvDesc = tmpSrvDesc;
		texOutput = texture;
		srvOutput = srv;
		return true;
	}
	bool ObjectNormalMapUpdater::LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texOutput, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOutput)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		return LoadTexture(device, context, filePath, texDesc, srvDesc, texOutput, srvOutput);
	}

	bool ObjectNormalMapUpdater::LoadTextureCPU(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
	{
		if (!device || !context || filePath.empty())
			return false;

		HRESULT hr;
		D3D11_TEXTURE2D_DESC tmpTexDesc;
		D3D11_SHADER_RESOURCE_VIEW_DESC tmpSrvDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		if (Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context))
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTextureFromFile(device, context, filePath, tmpTexDesc, tmpSrvDesc, DXGI_FORMAT_R8G8B8A8_UNORM, texture))
				return false;
		}
		else
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTexture2D(filePath, tmpTexDesc, tmpSrvDesc, DXGI_FORMAT_R8G8B8A8_UNORM, texture))
				return false;
		}

		tmpTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		tmpTexDesc.Usage = D3D11_USAGE_STAGING;
		tmpTexDesc.BindFlags = 0;
		tmpTexDesc.MiscFlags = 0;
		tmpTexDesc.MipLevels = 1;
		tmpTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
		hr = device->CreateTexture2D(&tmpTexDesc, nullptr, &stagingTexture);
		if (FAILED(hr))
		{
			logger::error("Failed to create staging texture ({}|{})", hr, filePath);
			return false;
		}
		texDesc = tmpTexDesc;
		srvDesc = tmpSrvDesc;
		output = stagingTexture;
		return CopySubresourceRegion(device, context, output.Get(), texture.Get(), 0, 0);
	}
	bool ObjectNormalMapUpdater::LoadTextureCPU(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		return LoadTextureCPU(device, context, filePath, texDesc, srvDesc, output);
	}

	DirectX::XMVECTOR ObjectNormalMapUpdater::SlerpVector(const DirectX::XMVECTOR& a, const DirectX::XMVECTOR& b, const float& t)
	{
		const float dotAB = std::clamp(DirectX::XMVectorGetX(DirectX::XMVector3Dot(a, b)), -1.0f, 1.0f);
		const float theta = acosf(dotAB) * t;
		const DirectX::XMVECTOR relVec = DirectX::XMVector3Normalize(
			DirectX::XMVectorSubtract(b, DirectX::XMVectorScale(a, dotAB))
		);
		return DirectX::XMVector3Normalize(
			DirectX::XMVectorAdd(
				DirectX::XMVectorScale(a, cosf(theta)),
				DirectX::XMVectorScale(relVec, sinf(theta))
			)
		);
	}

	bool ObjectNormalMapUpdater::ComputeBarycentric(const float& px, const float& py, const DirectX::XMINT2& a, const DirectX::XMINT2& b, const DirectX::XMINT2& c, DirectX::XMFLOAT3& out)
	{
		const DirectX::SimpleMath::Vector2 v0 = { (float)(b.x - a.x), (float)(b.y - a.y) };
		const DirectX::SimpleMath::Vector2 v1 = { (float)(c.x - a.x), (float)(c.y - a.y) };
		const DirectX::SimpleMath::Vector2 v2 = { px - a.x, py - a.y };

		const float d00 = v0.Dot(v0);
		const float d01 = v0.Dot(v1);
		const float d11 = v1.Dot(v1);
		const float d20 = v2.Dot(v0);
		const float d21 = v2.Dot(v1);
		const float denom = d00 * d11 - d01 * d01;

		if (denom == 0.0f)
			return false;

		const float v = (d11 * d20 - d01 * d21) / denom;
		const float w = (d00 * d21 - d01 * d20) / denom;
		const float u = 1.0f - v - w;

		if (u < 0 || v < 0 || w < 0)
			return false;

		out = { u, v, w };
		return true;
	}

	bool ObjectNormalMapUpdater::CreateStructuredBuffer(ID3D11Device* device, const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut)
	{
		if (!device)
			return false;

		D3D11_BUFFER_DESC desc = {};
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.ByteWidth = size;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = stride;
		desc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = data;

		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		auto hr = device->CreateBuffer(&desc, &initData, &buffer);
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
		hr = device->CreateShaderResourceView(buffer.Get(), &srvDesc, &srv);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create buffer shader resource view ({})", __func__, hr);
			return false;
		}

		bufferOut = buffer;
		srvOut = srv;
		return true;
	};

	bool ObjectNormalMapUpdater::BleedTexture(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		if (!device || !context)
			return false;

		bool isRendered = false;
		for (std::uint32_t i = 0; i < results.size(); i++)
		{
			if (results[i].existResource)
				continue;

			if (mergedTextureGeometries.find(results[i].geometry) != mergedTextureGeometries.end())
				continue;

			if (Config::GetSingleton().GetTextureMarginGPU())
				isRendered = BleedTextureGPU(device, context, resourceDatas[i],
								results[i].texture->normalmapShaderResourceView.Get(), results[i].texture->normalmapTexture2D.Get()) ? true : isRendered;
			else
				isRendered = BleedTexture(device, context, resourceDatas[i], results[i].texture->normalmapTexture2D.Get()) ? true : isRendered;
		}
		return isRendered;
	}
	bool ObjectNormalMapUpdater::BleedTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11Texture2D* texInOut)
	{
		if (!device || !context)
			return false;

		logger::info("{}::{} : Bleed texture...", __func__, resourceData->textureName);

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		const auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
			};
		const auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			};

		HRESULT hr;

		D3D11_TEXTURE2D_DESC stagingDesc = {};
		texInOut->GetDesc(&stagingDesc);
		stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.MiscFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		stagingDesc.ArraySize = 1;
		stagingDesc.SampleDesc.Count = 1;
		hr = device->CreateTexture2D(&stagingDesc, nullptr, &resourceData->bleedTextureData.texture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create staging texture ({})", __func__, hr);
			return false;
		}

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);

		UINT mipLevelEnd = stagingDesc.MipLevels;
		for (UINT mipLevel = 0; mipLevel < stagingDesc.MipLevels; mipLevel++) {
			const UINT width = std::max(stagingDesc.Width >> mipLevel, 1u);
			const UINT height = std::max(stagingDesc.Height >> mipLevel, 1u);
			if (width <= Config::GetSingleton().GetTextureMarginIgnoreSize() && height <= Config::GetSingleton().GetTextureMarginIgnoreSize())
			{
				mipLevelEnd = mipLevel;
				break;
			}
			CopySubresourceRegion(device, context, resourceData->bleedTextureData.texture2D.Get(), texInOut, mipLevel, mipLevel);
		}
		WaitForGPU(device, context).Wait();

		if (Config::GetSingleton().GetBleedTextureTime2())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

		for (UINT mipLevel = 0; mipLevel < mipLevelEnd; mipLevel++) {
			const UINT width = std::max(stagingDesc.Width >> mipLevel, 1u);
			const UINT height = std::max(stagingDesc.Height >> mipLevel, 1u); 
			const UINT radius = mipLevel / 2 + 1;

			std::vector<std::future<void>> processes;
			const std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(width), processingThreads->GetThreads() * 16));
			const std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(height), processingThreads->GetThreads() * 16));
			const std::size_t unitX = (std::size_t(width) + subX - 1) / subX;
			const std::size_t unitY = (std::size_t(height) + subY - 1) / subY;

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			ShaderLock();
			hr = context->Map(resourceData->bleedTextureData.texture2D.Get(), mipLevel, D3D11_MAP_READ_WRITE, 0, &mappedResource);
			ShaderUnlock();
			if (FAILED(hr)) {
				logger::error("{} : Failed to map staging texture ({})", __func__, hr);
				return false;
			}

			std::uint8_t* pData = reinterpret_cast<std::uint8_t*>(mappedResource.pData);

			struct ColorMap {
				std::uint32_t* src = nullptr;
				RGBA resultsColor;
			};
			concurrency::concurrent_vector<ColorMap> resultsColorMap;
			for (std::size_t px = 0; px < subX; px++) {
				for (std::size_t py = 0; py < subY; py++) {
					const std::size_t beginX = px * unitX;
					const std::size_t endX = std::min(beginX + unitX, std::size_t(width));
					const std::size_t beginY = py * unitY;
					const std::size_t endY = std::min(beginY + unitY, std::size_t(height));
					processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
						for (UINT y = beginY; y < endY; y++) {
							std::uint8_t* rowData = pData + y * mappedResource.RowPitch;
							for (UINT x = beginX; x < endX; ++x)
							{
								std::uint32_t* pixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
								RGBA color;
								color.SetReverse(*pixel);
								if (color.a == 1.0f)
									continue;

								RGBA averageColor(0.0f, 0.0f, 0.0f, 0.0f);
								std::uint8_t validCount = 0;
								for (INT r = 1; r <= radius; r++)
								{
									const INT nx_start = -r;
									const INT ny_start = -r;
									const INT nx_end = r;
									const INT ny_end = r;

									for (INT nx = nx_start; nx <= nx_end; nx++) {
										DirectX::XMINT2 nearCoord = { (INT)x + nx, (INT)y + ny_start };
										if (nearCoord.x > 0 && nearCoord.y > 0 &&
											nearCoord.x < width && nearCoord.y < height)
										{
											std::uint8_t* nearRowData = pData + nearCoord.y * mappedResource.RowPitch;
											std::uint32_t* nearPixel = reinterpret_cast<uint32_t*>(nearRowData + nearCoord.x * 4);
											RGBA nearColor;
											nearColor.SetReverse(*nearPixel);
											if (nearColor.a == 1.0f)
											{
												averageColor += nearColor;
												validCount++;
											}
										}

										nearCoord = { (INT)x + nx, (INT)y + ny_end };
										if (nearCoord.x > 0 && nearCoord.y > 0 &&
											nearCoord.x < width && nearCoord.y < height)
										{
											std::uint8_t* nearRowData = pData + nearCoord.y * mappedResource.RowPitch;
											std::uint32_t* nearPixel = reinterpret_cast<uint32_t*>(nearRowData + nearCoord.x * 4);
											RGBA nearColor;
											nearColor.SetReverse(*nearPixel);
											if (nearColor.a == 1.0f)
											{
												averageColor += nearColor;
												validCount++;
											}
										}
									}
									for (INT ny = ny_start + 1; ny < ny_end; ny++) {
										DirectX::XMINT2 nearCoord = { (INT)x + nx_start, (INT)y + ny };
										if (nearCoord.x > 0 && nearCoord.y > 0 &&
											nearCoord.x < width && nearCoord.y < height)
										{
											std::uint8_t* nearRowData = pData + nearCoord.y * mappedResource.RowPitch;
											std::uint32_t* nearPixel = reinterpret_cast<uint32_t*>(nearRowData + nearCoord.x * 4);
											RGBA nearColor;
											nearColor.SetReverse(*nearPixel);
											if (nearColor.a == 1.0f)
											{
												averageColor += nearColor;
												validCount++;
											}
										}

										nearCoord = { (INT)x + nx_end, (INT)y + ny };
										if (nearCoord.x > 0 && nearCoord.y > 0 &&
											nearCoord.x < width && nearCoord.y < height)
										{
											std::uint8_t* nearRowData = pData + nearCoord.y * mappedResource.RowPitch;
											std::uint32_t* nearPixel = reinterpret_cast<uint32_t*>(nearRowData + nearCoord.x * 4);
											RGBA nearColor;
											nearColor.SetReverse(*nearPixel);
											if (nearColor.a == 1.0f)
											{
												averageColor += nearColor;
												validCount++;
											}
										}
									}
									if (validCount > 0)
										break;
								}

								if (validCount == 0)
									continue;

								RGBA resultsColor = averageColor / validCount;
								resultsColorMap.push_back(ColorMap{ pixel, resultsColor });
							}
						}
					}));
				}
			}
			for (auto& process : processes)
			{
				process.get();
			}
			processes.clear();

			std::size_t sub = std::max(std::size_t(1), std::min(resultsColorMap.size(), processingThreads->GetThreads()));
			std::size_t unit = (resultsColorMap.size() + sub - 1) / sub;
			for (std::size_t p = 0; p < sub; p++) {
				std::size_t begin = p * unit;
				std::size_t end = std::min(begin + unit, resultsColorMap.size());
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++) {
						*resultsColorMap[i].src = resultsColorMap[i].resultsColor.GetReverse();
					}
				}));
			}
			for (auto& process : processes)
			{
				process.get();
			}

			ShaderLock();
			context->Unmap(resourceData->bleedTextureData.texture2D.Get(), mipLevel);
			ShaderUnlock();
		}

		if (Config::GetSingleton().GetBleedTextureTime2())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);

		for (UINT mipLevel = 0; mipLevel < mipLevelEnd; mipLevel++) 
		{
			CopySubresourceRegion(device, context, texInOut, resourceData->bleedTextureData.texture2D.Get(), mipLevel, mipLevel);
		}
		logger::debug("{}::{} : Bleed texture done", __func__, resourceData->textureName);
		return true;
	}
	bool ObjectNormalMapUpdater::BleedTextureGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* srvInOut, ID3D11Texture2D* texInOut)
	{
		const std::string_view _func_ = __func__;
		if (!device || !context)
			return false;

		logger::info("{}::{} : Bleed texture...", _func_, resourceData->textureName);

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, false, false);

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		const auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		const auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		const auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(device, BleedTextureShaderName.data());
		if (!shader)
			return false;

		HRESULT hr;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvInOut->GetDesc(&srvDesc);
		D3D11_TEXTURE2D_DESC desc;
		texInOut->GetDesc(&desc);

		//create buffers
		struct ConstBufferData
		{
			UINT width;
			UINT height;
			UINT widthStart;
			UINT heightStart;

			UINT mipLevel;
			UINT radius;
			UINT padding2;
			UINT padding3;
		};
		static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");

		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.ByteWidth = sizeof(ConstBufferData);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = 0;
		cbDesc.MiscFlags = 0;
		cbDesc.StructureByteStride = 0;

		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = 0;
		desc.CPUAccessFlags = 0;

		hr = device->CreateTexture2D(&desc, nullptr, &resourceData->bleedTextureData.texture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create texture 2d 1 ({})", _func_, hr);
			return false;
		}

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, true, false);

		struct ShaderBackup {
		public:
			void Backup(ID3D11DeviceContext* context) {
				context->CSGetShader(&shader, nullptr, 0);
				context->CSGetConstantBuffers(0, 1, &constBuffer);
				context->CSGetShaderResources(0, 1, &srv);
				context->CSGetUnorderedAccessViews(0, 1, &uav);
				Unbind(context);
			}
			void Revert(ID3D11DeviceContext* context) {
				Unbind(context);
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, srv.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
			}
		private:
			void Unbind(ID3D11DeviceContext* context) {
				ID3D11Buffer* emptyBuffer[1] = { nullptr };
				ID3D11ShaderResourceView* emptySRV[1] = { nullptr };
				ID3D11UnorderedAccessView* emptyUAV[1] = { nullptr };
				context->CSSetShader(nullptr, nullptr, 0);
				context->CSSetConstantBuffers(0, 1, emptyBuffer);
				context->CSSetShaderResources(0, 1, emptySRV);
				context->CSSetUnorderedAccessViews(0, 1, emptyUAV, nullptr);
			}

			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		};

		UINT mipLevelEnd = desc.MipLevels;
		if (isSecondGPU || isNoSplitGPU)
		{
			for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++) 
			{
				const UINT width = std::max(desc.Width >> mipLevel, 1u);
				const UINT height = std::max(desc.Height >> mipLevel, 1u);
				if (width <= Config::GetSingleton().GetTextureMarginIgnoreSize() && height <= Config::GetSingleton().GetTextureMarginIgnoreSize())
				{
					mipLevelEnd = mipLevel;
					break;
				}

				const DirectX::XMUINT2 dispatch = { (width + 8 - 1) / 8, (height + 8 - 1) / 8 };

				ConstBufferData cbData = {};
				cbData.height = height;
				cbData.width = width;
				cbData.heightStart = 0;
				cbData.widthStart = 0;
				cbData.mipLevel = mipLevel;
				cbData.radius = mipLevel / 2 + 1;

				D3D11_SUBRESOURCE_DATA cbInitData = {};
				cbInitData.pSysMem = &cbData;
				Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
				hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
				if (FAILED(hr)) {
					logger::error("{} : Failed to create const buffer ({})", _func_, hr);
					return false;
				}
				resourceData->bleedTextureData.constBuffers.push_back(constBuffer);

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = mipLevel;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
				hr = device->CreateUnorderedAccessView(resourceData->bleedTextureData.texture2D.Get(), &uavDesc, &uav);
				if (FAILED(hr)) {
					logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
					return false;
				}
				resourceData->bleedTextureData.uavs.push_back(uav);

				if (Config::GetSingleton().GetBleedTextureTime2())
					GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
									  + "::" + std::to_string(mipLevel), false, false);
				ShaderBackup sb;
				ShaderLock();
				sb.Backup(context);
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, &srvInOut);
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
				context->Dispatch(dispatch.x, dispatch.y, 1);
				sb.Revert(context);
				ShaderUnlock();

				if (Config::GetSingleton().GetBleedTextureTime2())
					GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
									  + "::" + std::to_string(mipLevel), true, false);
			}
		}
		else
		{
			const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
			std::vector<std::future<void>> gpuTasks;

			for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
			{
				const UINT width = std::max(desc.Width >> mipLevel, 1u);
				const UINT height = std::max(desc.Height >> mipLevel, 1u);
				if (width <= Config::GetSingleton().GetTextureMarginIgnoreSize() && height <= Config::GetSingleton().GetTextureMarginIgnoreSize())
				{
					mipLevelEnd = mipLevel;
					break;
				}

				const DirectX::XMUINT2 dispatch = { (width + 8 - 1) / 8, (height + 8 - 1) / 8 };
				const std::uint32_t subXSize = std::max(1u, width / subResolution);
				const std::uint32_t subYSize = std::max(1u, height / subResolution);

				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = mipLevel;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
				hr = device->CreateUnorderedAccessView(resourceData->bleedTextureData.texture2D.Get(), &uavDesc, &uav);
				if (FAILED(hr)) {
					logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
					return false;
				}
				resourceData->bleedTextureData.uavs.push_back(uav);

				ConstBufferData cbData = {};
				cbData.width = width;
				cbData.height = height;
				cbData.mipLevel = mipLevel;
				cbData.radius = mipLevel / 2 + 1;

				for (std::uint32_t subY = 0; subY < subYSize; subY++)
				{
					for (std::uint32_t subX = 0; subX < subXSize; subX++)
					{
						ConstBufferData cbData_ = cbData;
						cbData_.widthStart = subResolution * subX;
						cbData_.heightStart = subResolution * subY;
						D3D11_SUBRESOURCE_DATA cbInitData = {};
						cbInitData.pSysMem = &cbData_;
						Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
						hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
						if (FAILED(hr)) {
							logger::error("{} : Failed to create const buffer ({})", _func_, hr);
							return false;
						}
						resourceData->bleedTextureData.constBuffers.push_back(constBuffer);

						auto gpuFunc = [&, dispatch, constBuffer, width, height, mipLevel, subX, subY, uav]() {
							if (Config::GetSingleton().GetBleedTextureTime2())
								GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
												  + "::" + std::to_string(subX) + "|" + std::to_string(subY) + "::" + std::to_string(mipLevel), false, false);

							ShaderBackup sb;
							ShaderLock();
							sb.Backup(context);
							context->CSSetShader(shader.Get(), nullptr, 0);
							context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
							context->CSSetShaderResources(0, 1, &srvInOut);
							context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
							context->Dispatch(dispatch.x, dispatch.y, 1);
							sb.Revert(context);
							ShaderUnlock();

							if (Config::GetSingleton().GetBleedTextureTime2())
								GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
												  + "::" + std::to_string(subX) + "|" + std::to_string(subY) + "::" + std::to_string(mipLevel), true, false);
						};

						if (width <= 256 && height <= 256)
						{
							gpuFunc();
						}
						else
						{
							gpuTasks.push_back(gpuTask->submitAsync([gpuFunc]() {
								gpuFunc();
							}));
						}
					}
				}
			}
			for (auto& task : gpuTasks)
			{
				task.get();
			}
		}
		WaitForGPU(device, context).Wait();
		for (UINT mipLevel = 0; mipLevel < mipLevelEnd; mipLevel++)
		{
			CopySubresourceRegion(device, context, texInOut, resourceData->bleedTextureData.texture2D.Get(), mipLevel, mipLevel);
		}
		logger::debug("{}::{} : Bleed texture done", _func_, resourceData->textureName);

		return true;
	}

	bool ObjectNormalMapUpdater::MergeTexture(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		if (!device || !context)
			return false;

		bool merged = false;
		for (std::uint32_t dst = 0; dst < results.size(); dst++)
		{
			if (mergedTextureGeometries.find(results[dst].geometry) != mergedTextureGeometries.end())
				continue;
			for (std::uint32_t src = 0; src < results.size(); src++)
			{
				if (results[src].geometry == results[dst].geometry)
					continue;
				if (results[src].textureName != results[dst].textureName)
					continue;

				if (results[dst].existResource || results[src].existResource)
				{
					if (results[dst].existResource)
					{
						results[src].texture = results[dst].texture;
					}
					else
					{
						results[dst].texture = results[src].texture;
					}
					mergedTextureGeometries.insert(results[src].geometry);
					continue;
				}

				logger::info("{} : Merge texture {} into {}...", results[src].textureName, results[src].geoName, results[dst].geoName);
				bool mergeResult = false;
				if (Config::GetSingleton().GetMergeTextureGPU())
					mergeResult = MergeTextureGPU(device, context, resourceDatas[src],
												  results[dst].texture->normalmapShaderResourceView.Get(), results[dst].texture->normalmapTexture2D.Get(),
												  results[src].texture->normalmapShaderResourceView.Get(), results[src].texture->normalmapTexture2D.Get());
				else
					mergeResult = MergeTexture(device, context, resourceDatas[src], 
											   results[dst].texture->normalmapTexture2D.Get(), results[src].texture->normalmapTexture2D.Get());
				if (mergeResult)
				{
					results[src].texture = results[dst].texture;
					mergedTextureGeometries.insert(results[src].geometry);
					merged = true;
				}
			}
		}
		return merged;
	}
	bool ObjectNormalMapUpdater::MergeTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11Texture2D* dstTex, ID3D11Texture2D* srcTex)
	{
		if (!device || !context || !dstTex || !srcTex)
			return false;

		logger::info("{}::{} : Merge texture...", __func__, resourceData->textureName);

		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		HRESULT hr;

		D3D11_TEXTURE2D_DESC dstStagingDesc = {}, srcStagingDesc = {};
		dstTex->GetDesc(&dstStagingDesc);
		dstStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		dstStagingDesc.Usage = D3D11_USAGE_STAGING;
		dstStagingDesc.BindFlags = 0;
		dstStagingDesc.MiscFlags = 0;
		dstStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dstStagingDesc.MipLevels = 1;
		dstStagingDesc.ArraySize = 1;
		dstStagingDesc.SampleDesc.Count = 1;
		srcTex->GetDesc(&srcStagingDesc);
		srcStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srcStagingDesc.Usage = D3D11_USAGE_STAGING;
		srcStagingDesc.BindFlags = 0;
		srcStagingDesc.MiscFlags = 0;
		srcStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		srcStagingDesc.MipLevels = 1;
		srcStagingDesc.ArraySize = 1;
		srcStagingDesc.SampleDesc.Count = 1;
		hr = device->CreateTexture2D(&dstStagingDesc, nullptr, &resourceData->mergeTextureData.dstTexture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create staging texture ({})", __func__, hr);
			return false;
		}
		hr = device->CreateTexture2D(&srcStagingDesc, nullptr, &resourceData->mergeTextureData.srcTexture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create staging texture ({})", __func__, hr);
			return false;
		}

		CopySubresourceRegion(device, context, resourceData->mergeTextureData.dstTexture2D.Get(), dstTex, 0, 0);
		CopySubresourceRegion(device, context, resourceData->mergeTextureData.srcTexture2D.Get(), srcTex, 0, 0);
		WaitForGPU(device, context).Wait();

		D3D11_MAPPED_SUBRESOURCE dstMappedResource, srcMappedResource;
		ShaderLock();
		hr = context->Map(resourceData->mergeTextureData.dstTexture2D.Get(), 0, D3D11_MAP_WRITE, 0, &dstMappedResource);
		hr = context->Map(resourceData->mergeTextureData.srcTexture2D.Get(), 0, D3D11_MAP_READ, 0, &srcMappedResource);
		ShaderUnlock();

		std::uint8_t* dst_pData = reinterpret_cast<std::uint8_t*>(dstMappedResource.pData);
		std::uint8_t* src_pData = reinterpret_cast<std::uint8_t*>(srcMappedResource.pData);
		std::vector<std::future<void>> processes;
		const std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(dstStagingDesc.Width), processingThreads->GetThreads() * 8));
		const std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(dstStagingDesc.Height), processingThreads->GetThreads() * 8));
		const std::size_t unitX = (std::size_t(dstStagingDesc.Width) + subX - 1) / subX;
		const std::size_t unitY = (std::size_t(dstStagingDesc.Height) + subY - 1) / subY;
		for (std::size_t px = 0; px < subX; px++) {
			for (std::size_t py = 0; py < subY; py++) {
				std::size_t beginX = px * unitX;
				std::size_t endX = std::min(beginX + unitX, std::size_t(dstStagingDesc.Width));
				std::size_t beginY = py * unitY;
				std::size_t endY = std::min(beginY + unitY, std::size_t(dstStagingDesc.Height));
				processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
					for (UINT dstY = beginY; dstY < endY; dstY++) {
						std::uint8_t* dstRowData = dst_pData + dstY * dstMappedResource.RowPitch;
						const UINT srcY = std::min(srcStagingDesc.Height, srcStagingDesc.Height * (dstY / dstStagingDesc.Height));
						std::uint8_t* srcRowData = src_pData + srcY * srcMappedResource.RowPitch;
						for (UINT dstX = beginX; dstX < endX; ++dstX)
						{
							std::uint32_t* dstPixel = reinterpret_cast<uint32_t*>(dstRowData + dstX * 4);
							const UINT srcX = std::min(srcStagingDesc.Width, srcStagingDesc.Width * (dstX / dstStagingDesc.Width));
							std::uint32_t* srcPixel = reinterpret_cast<uint32_t*>(srcRowData + srcX * 4);
							RGBA dstPixelColor, srcPixelColor;
							dstPixelColor.SetReverse(*dstPixel);
							srcPixelColor.SetReverse(*srcPixel);
							if (dstPixelColor.a < 1.0f && srcPixelColor.a < 1.0f)
								continue;

							RGBA dstColor = RGBA::lerp(srcPixelColor, srcPixelColor, srcPixelColor.a);
							*dstPixel = dstColor.GetReverse() | 0xFF000000;
						}
					}
				}));
			}
		}
		for (auto& process : processes)
		{
			process.get();
		}

		ShaderLock();
		context->Unmap(resourceData->mergeTextureData.dstTexture2D.Get(), 0);
		context->Unmap(resourceData->mergeTextureData.srcTexture2D.Get(), 0);
		ShaderUnlock();
		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);
		WaitForGPU(device, context).Wait();

		CopySubresourceRegion(device, context, dstTex, resourceData->mergeTextureData.dstTexture2D.Get(), 0, 0);
		logger::debug("{}::{} : Merge texture done", __func__, resourceData->textureName);
		return true;
	}
	bool ObjectNormalMapUpdater::MergeTextureGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* dstSrv, ID3D11Texture2D* dstTex, ID3D11ShaderResourceView* srcSrv, ID3D11Texture2D* srcTex)
	{
		if (!device || !context)
			return false;

		const auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(device, MergeTextureShaderName.data());
		if (!shader)
			return false;

		const std::string_view _func_ = __func__;
		logger::info("{}::{} : Merge texture...", _func_, resourceData->textureName);

		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, false, false);

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		HRESULT hr;

		D3D11_TEXTURE2D_DESC desc;
		dstTex->GetDesc(&desc);

		//create buffers
		struct ConstBufferData
		{
			UINT width;
			UINT height;
			UINT widthStart;
			UINT heightStart;
		};
		static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");

		struct ShaderBackup {
		public:
			void Backup(ID3D11DeviceContext* context) {
				context->CSGetShader(&shader, nullptr, 0);
				context->CSGetConstantBuffers(0, 1, &constBuffer);
				context->CSGetShaderResources(0, 1, &srv1);
				context->CSGetShaderResources(1, 1, &srv2);
				context->CSGetUnorderedAccessViews(0, 1, &uav);
				Unbind(context);
			}
			void Revert(ID3D11DeviceContext* context) {
				Unbind(context);
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, srv1.GetAddressOf());
				context->CSSetShaderResources(1, 1, srv2.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
			}
		private:
			void Unbind(ID3D11DeviceContext* context) {
				ID3D11Buffer* emptyBuffer[1] = { nullptr };
				ID3D11ShaderResourceView* emptySRV[2] = { nullptr };
				ID3D11UnorderedAccessView* emptyUAV[1] = { nullptr };
				context->CSSetShader(nullptr, nullptr, 0);
				context->CSSetConstantBuffers(0, 1, emptyBuffer);
				context->CSSetShaderResources(0, 2, emptySRV);
				context->CSSetUnorderedAccessViews(0, 1, emptyUAV, nullptr);
			}

			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv1;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv2;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		};

		const UINT width = desc.Width;
		const UINT height = desc.Height;

		ConstBufferData cbData = {};
		cbData.width = width;
		cbData.height = height;
		cbData.widthStart = 0;
		cbData.heightStart = 0;

		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.ByteWidth = sizeof(ConstBufferData);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = 0;
		cbDesc.MiscFlags = 0;
		cbDesc.StructureByteStride = 0;

		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.MiscFlags = 0;
		desc.MipLevels = 1;
		desc.CPUAccessFlags = 0;
		hr = device->CreateTexture2D(&desc, nullptr, &resourceData->mergeTextureData.dstTexture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create texture 2d ({})", _func_, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		hr = device->CreateUnorderedAccessView(resourceData->mergeTextureData.dstTexture2D.Get(), &uavDesc, &resourceData->mergeTextureData.uav);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}

		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, true, false);

		if (isSecondGPU || isNoSplitGPU)
		{
			const DirectX::XMUINT2 dispatch = { (width + 8 - 1) / 8, (height + 8 - 1) / 8 };

			D3D11_SUBRESOURCE_DATA cbInitData = {};
			cbInitData.pSysMem = &cbData;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
			if (FAILED(hr)) {
				logger::error("{} : Failed to create const buffer ({})", _func_, hr);
				return false;
			}

			if (Config::GetSingleton().GetMergeTime2())
				GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
								  , false, false);

			ShaderBackup sb;
			ShaderLock();
			sb.Backup(context);
			context->CSSetShader(shader.Get(), nullptr, 0);
			context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
			context->CSSetShaderResources(0, 1, &srcSrv);
			context->CSSetShaderResources(1, 1, &dstSrv);
			context->CSSetUnorderedAccessViews(0, 1, resourceData->mergeTextureData.uav.GetAddressOf(), nullptr);
			context->Dispatch(dispatch.x, dispatch.y, 1);
			sb.Revert(context);
			ShaderUnlock();

			if (Config::GetSingleton().GetMergeTime2())
				GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
								  , true, false);

			resourceData->mergeTextureData.constBuffers.push_back(constBuffer);
		}
		else
		{
			const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
			const DirectX::XMUINT2 dispatch = { (std::min(width, subResolution) + 8 - 1) / 8, (std::min(height, subResolution) + 8 - 1) / 8 };
			const std::uint32_t subXSize = std::max(1u, width / subResolution);
			const std::uint32_t subYSize = std::max(1u, height / subResolution);

			bool isResultFirst = true;
			std::vector<std::future<void>> gpuTasks;
			for (std::uint32_t subY = 0; subY < subYSize; subY++)
			{
				for (std::uint32_t subX = 0; subX < subXSize; subX++)
				{
					auto cbData_ = cbData;
					cbData_.widthStart = subResolution * subX;
					cbData_.heightStart = subResolution * subY;
					D3D11_SUBRESOURCE_DATA cbInitData = {};
					cbInitData.pSysMem = &cbData_;
					Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
					hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
					if (FAILED(hr)) {
						logger::error("{} : Failed to create const buffer ({})", _func_, hr);
						return false;
					}

					gpuTasks.push_back(gpuTask->submitAsync([&, constBuffer, subX, subY]() {
						if (Config::GetSingleton().GetMergeTime2())
							GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
											  + "::" + std::to_string(subX) + "|" + std::to_string(subY), false, false);

						ShaderBackup sb;
						ShaderLock();
						sb.Backup(context);
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						context->CSSetShaderResources(0, 1, &srcSrv);
						context->CSSetShaderResources(1, 1, &dstSrv);
						context->CSSetUnorderedAccessViews(0, 1, resourceData->mergeTextureData.uav.GetAddressOf(), nullptr);
						context->Dispatch(dispatch.x, dispatch.y, 1);
						sb.Revert(context);
						ShaderUnlock();

						if (Config::GetSingleton().GetMergeTime2())
							GPUPerformanceLog(device, context, std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
											  + "::" + std::to_string(subX) + "|" + std::to_string(subY), true, false);
					}));
					resourceData->mergeTextureData.constBuffers.push_back(constBuffer);
				}
			}
			for (auto& task : gpuTasks)
			{
				task.get();
			}
		}
		WaitForGPU(device, context).Wait();

		CopySubresourceRegion(device, context, dstTex, resourceData->mergeTextureData.dstTexture2D.Get(), 0, 0);
		logger::debug("{}::{} : Merge texture done", _func_, resourceData->textureName);
		return true;
	}

	bool ObjectNormalMapUpdater::GenerateMipMap(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		if (!device || !context || !Config::GetSingleton().GetUseMipMap())
			return false;

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		const auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		const auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		for (std::uint32_t i = 0; i < results.size(); i++)
		{
			if (results[i].existResource)
				continue;

			if (mergedTextureGeometries.find(results[i].geometry) != mergedTextureGeometries.end())
				continue;
			ShaderLock();
			context->GenerateMips(results[i].texture->normalmapShaderResourceView.Get());
			ShaderUnlock();
		}
		return true;
	}

	bool ObjectNormalMapUpdater::CopySubresourceRegion(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* dstTexture, ID3D11Texture2D* srcTexture, UINT dstMipMapLevel, UINT srcMipMapLevel)
	{
		if (!device || !context || !dstTexture || !srcTexture)
			return false;

		D3D11_TEXTURE2D_DESC dstDesc, srcDesc;
		dstTexture->GetDesc(&dstDesc);
		srcTexture->GetDesc(&srcDesc);

		const UINT srcWidth = std::max(srcDesc.Width >> srcMipMapLevel, 1u);
		const UINT srcHeight = std::max(srcDesc.Height >> srcMipMapLevel, 1u);
		const UINT dstWidth = std::max(dstDesc.Width >> dstMipMapLevel, 1u);
		const UINT dstHeight = std::max(dstDesc.Height >> dstMipMapLevel, 1u);
		if (srcWidth != dstWidth || srcHeight != dstHeight)
			return false;

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		if (isSecondGPU || isNoSplitGPU || (dstWidth <= 256 && dstHeight <= 256))
		{
			ShaderLock();
			context->CopySubresourceRegion(dstTexture, dstMipMapLevel, 0, 0, 0, srcTexture, srcMipMapLevel, NULL);
			ShaderUnlock();
			return true;
		}

		const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
		const std::uint32_t subXSize = std::max(1u, dstWidth / subResolution);
		const std::uint32_t subYSize = std::max(1u, dstHeight / subResolution);
		std::vector<std::future<void>> gpuTasks;
		for (std::uint32_t subY = 0; subY < subYSize; subY++)
		{
			for (std::uint32_t subX = 0; subX < subXSize; subX++)
			{
				D3D11_BOX box = {};
				box.left = subX * subResolution;
				box.right = std::min(dstWidth, box.left + subResolution);
				box.top = subY * subResolution;
				box.bottom = std::min(dstHeight, box.top + subResolution);
				box.front = 0;
				box.back = 1;

				gpuTasks.push_back(gpuTask->submitAsync([&, box, dstWidth, dstHeight, subX, subY]() {
					if (Config::GetSingleton().GetTextureCopyTime())
						GPUPerformanceLog(device, context, std::string("CopySubresourceRegion") + "::" + std::to_string(dstWidth) + "::" + std::to_string(dstHeight)
										  + "::" + std::to_string(subX) + "|" + std::to_string(subY), false, false);

					ShaderLock();
					context->CopySubresourceRegion(
						dstTexture, dstMipMapLevel,
						box.left, box.top, 0,
						srcTexture, srcMipMapLevel,
						&box);
					ShaderUnlock();

					if (Config::GetSingleton().GetTextureCopyTime())
						GPUPerformanceLog(device, context, std::string("CopySubresourceRegion") + "::" + std::to_string(dstWidth) + "::" + std::to_string(dstHeight)
										  + "::" + std::to_string(subX) + "|" + std::to_string(subY), true, false);
				}));
			}
		}
		for (auto& task : gpuTasks) {
			task.get();
		}
		return true;
	}

	void ObjectNormalMapUpdater::CopyResourceToMain(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		if (!device || !context)
			return;

		if (Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context))
		{
			WaitForGPU(device, context, false).Wait();
			std::unordered_set<std::uint32_t> failedCopyResources;
			for (std::uint32_t i = 0; i < results.size(); i++)
			{
				if (results[i].existResource)
					continue;
				if (mergedTextureGeometries.find(results[i].geometry) != mergedTextureGeometries.end())
					continue;
				if (CopyResourceSecondToMain(resourceDatas[i], results[i].texture->normalmapTexture2D, results[i].texture->normalmapShaderResourceView))
				{
					logger::info("{} : normalmap created", results[i].textureName, results[i].geoName);
					NormalMapStore::GetSingleton().AddResource(results[i].hash, results[i].texture);
				}
				else
					failedCopyResources.insert(i);
			}
			auto results_ = results;
			results.clear();
			for (std::uint32_t i = 0; i < results_.size(); i++)
			{
				if (failedCopyResources.find(i) != failedCopyResources.end())
					continue;
				results.push_back(results_[i]);
			}
		}
		else
		{
			if (Config::GetSingleton().GetWaitForRendererTickMS() > 0)
			{
				for (std::uint32_t i = 0; i < results.size(); i++)
				{
					if (results[i].existResource)
						continue;

					resourceDatas[i]->GetQuery(device, context);
					ResourceDataMapLock.lock_shared();
					ResourceDataMap.push_back(resourceDatas[i]);
					ResourceDataMapLock.unlock_shared();

					NormalMapStore::GetSingleton().AddResource(results[i].hash, results[i].texture);
					logger::info("{} : normalmap created", results[i].textureName, results[i].geoName);
				}
			}
		}
	}
	bool ObjectNormalMapUpdater::CopyResourceSecondToMain(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut)
	{
		if (!Shader::ShaderManager::GetSingleton().IsValidSecondGPU() || !texInOut || !srvInOut)
			return false;

		logger::info("{}::{} : Copy resource from sub to main GPU...", __func__, resourceData->textureName);

		resourceData->copySecondToMainData.srcOldTexture2D = texInOut;
		resourceData->copySecondToMainData.srcOldShaderResourceView = srvInOut;

		D3D11_TEXTURE2D_DESC texDesc, copyDesc;
		resourceData->copySecondToMainData.srcOldTexture2D->GetDesc(&texDesc);
		copyDesc = texDesc;
		copyDesc.Usage = D3D11_USAGE_STAGING;
		copyDesc.BindFlags = 0;
		copyDesc.MiscFlags = 0;
		copyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		resourceData->copySecondToMainData.srcOldShaderResourceView->GetDesc(&srvDesc);

		HRESULT hr;

		hr = Shader::ShaderManager::GetSingleton().GetSecondDevice()->CreateTexture2D(&copyDesc, nullptr, &resourceData->copySecondToMainData.copyTexture2D);
		if (FAILED(hr)) {
			logger::error("Failed to create copy texture : {}", hr);
			return false;
		}

		Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
		Shader::ShaderManager::GetSingleton().GetSecondContext()->CopyResource(resourceData->copySecondToMainData.copyTexture2D.Get(), resourceData->copySecondToMainData.srcOldTexture2D.Get());
		Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();

		WaitForGPU(Shader::ShaderManager::GetSingleton().GetSecondDevice(), Shader::ShaderManager::GetSingleton().GetSecondContext()).Wait();

		std::vector<D3D11_SUBRESOURCE_DATA> initDatas(texDesc.MipLevels);
		std::vector<std::vector<std::uint8_t>> buffers(texDesc.MipLevels);

		for (UINT mipLevel = 0; mipLevel < texDesc.MipLevels; mipLevel++) {
			const UINT height = std::max(texDesc.Height >> mipLevel, 1u);
			std::size_t blockHeight = 0;

			D3D11_MAPPED_SUBRESOURCE mapped;
			Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			hr = Shader::ShaderManager::GetSingleton().GetSecondContext()->Map(resourceData->copySecondToMainData.copyTexture2D.Get(), mipLevel, D3D11_MAP_READ, 0, &mapped);
			Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to map copy texture : {}", hr);
				return false;
			}

			const std::size_t rowPitch = mapped.RowPitch;

			if (texDesc.Format == DXGI_FORMAT_BC7_UNORM || texDesc.Format == DXGI_FORMAT_BC3_UNORM || texDesc.Format == DXGI_FORMAT_BC1_UNORM)
			{
				blockHeight = std::size_t(height + 3) / 4;
			}
			else if (texDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
			{
				blockHeight = height;
			}
			else
			{
				blockHeight = height;
			}

			buffers[mipLevel].resize(rowPitch * blockHeight);
			for (std::size_t y = 0; y < blockHeight; y++) {
				memcpy(buffers[mipLevel].data() + y * rowPitch, reinterpret_cast<std::uint8_t*>(mapped.pData) + y * rowPitch, rowPitch);
			}

			Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			Shader::ShaderManager::GetSingleton().GetSecondContext()->Unmap(resourceData->copySecondToMainData.copyTexture2D.Get(), mipLevel);
			Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();

			initDatas[mipLevel].pSysMem = buffers[mipLevel].data();
			initDatas[mipLevel].SysMemPitch = rowPitch;
			initDatas[mipLevel].SysMemSlicePitch = 0;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
		hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateTexture2D(&texDesc, initDatas.data(), &texture2D);
		if (FAILED(hr)) {
			logger::error("Failed to create dst texture : {}", hr);
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateShaderResourceView(texture2D.Get(), &srvDesc, &srv);
		if (FAILED(hr)) {
			logger::error("Failed to create dst shader resource view : {}", hr);
			return false;
		}

		texInOut = texture2D;
		srvInOut = srv;
		logger::debug("{}::{} : Copy resource from second to main GPU done", __func__, resourceData->textureName);
		return true;
	}

	bool ObjectNormalMapUpdater::CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		if (!device || !context)
			return false;

		bool isCompressed = false;
		for (std::uint32_t i = 0; i < results.size(); i++)
		{
			if (results[i].existResource)
				continue;
			if (mergedTextureGeometries.find(results[i].geometry) != mergedTextureGeometries.end())
				continue;
			isCompressed = CompressTexture(device, context,
							resourceDatas[i], results[i].texture->normalmapTexture2D, results[i].texture->normalmapShaderResourceView) ? true : isCompressed;
		}
		return isCompressed;
	}
	bool ObjectNormalMapUpdater::CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut)
	{
		if (Config::GetSingleton().GetTextureCompress() == 0 || !device || !context || !texInOut || !srvInOut)
			return false;

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		DXGI_FORMAT format = DXGI_FORMAT_BC7_UNORM;
		if (Config::GetSingleton().GetTextureCompress() == -1)
		{
			if (isSecondGPU)
				format = DXGI_FORMAT_BC7_UNORM;
			else
				return false;
		}
		else
		{
			if (Config::GetSingleton().GetTextureCompress() == 1)
				format = DXGI_FORMAT_BC3_UNORM;
			else if (Config::GetSingleton().GetTextureCompress() == 2)
				format = DXGI_FORMAT_BC7_UNORM;
			else
				return false;
		}

		logger::info("{}::{} : Compress texture... {}", __func__, resourceData->textureName, magic_enum::enum_name(format).data());

		resourceData->textureCompressData.srcOldTexture2D = texInOut;
		resourceData->textureCompressData.srcOldShaderResourceView = srvInOut;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture = nullptr;
		bool isCompressed = false;
		auto compressTaskFunc = [&]() {
			if (Config::GetSingleton().GetCompressTime())
				PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

			isCompressed = Shader::TextureLoadManager::GetSingleton().CompressTexture(device, context, resourceData->textureCompressData.srcOldTexture2D, format, dstTexture);

			if (Config::GetSingleton().GetCompressTime())
				PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);
			};

		if (isSecondGPU || isNoSplitGPU)
		{
			compressTaskFunc();
		}
		else
		{
			gpuTask->submitAsync([compressTaskFunc]() {
				compressTaskFunc();
			}).get();
		}

		if (!isCompressed || !dstTexture)
			return false;

		D3D11_TEXTURE2D_DESC desc;
		dstTexture->GetDesc(&desc);
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		resourceData->textureCompressData.srcOldShaderResourceView->GetDesc(&srvDesc);
		srvDesc.Format = desc.Format;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstSrv;
		auto hr = device->CreateShaderResourceView(dstTexture.Get(), &srvDesc, &dstSrv);
		if (FAILED(hr)) {
			logger::error("{}::{} : Failed to create ShaderResourceView ({})", __func__, resourceData->textureName, hr);
			return false;
		}

		texInOut = dstTexture;
		srvInOut = dstSrv;

		logger::debug("{}::{} : Compress texture done", __func__, resourceData->textureName);
		return isCompressed;
	}

	void ObjectNormalMapUpdater::GPUPerformanceLog(ID3D11Device* device, ID3D11DeviceContext* context, std::string funcStr, bool isEnd, bool isAverage, std::uint32_t args)
	{
		if (!PerformanceCheck)
			return;

		struct GPUTimer
		{
			Microsoft::WRL::ComPtr<ID3D11Query> startQuery;
			Microsoft::WRL::ComPtr<ID3D11Query> endQuery;
			Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
		};

		if (!device || !context)
			return;

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		auto ShaderLock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
		};
		auto ShaderUnlock = [isSecondGPU]() {
			if (isSecondGPU)
				Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
			else
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		};

		static std::unordered_map<std::string, GPUTimer> gpuTimers;
		static std::unordered_map<std::string, double> funcAverageArgs;
		static std::unordered_map<std::string, unsigned> funcAverageCount;
		static std::mutex logLock;

		std::lock_guard<std::mutex> lg(logLock);

		if (!isEnd)
		{
			auto found = gpuTimers.find(funcStr);
			if (found == gpuTimers.end()) {
				D3D11_QUERY_DESC desc = {};
				GPUTimer timer;

				desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
				device->CreateQuery(&desc, &timer.disjointQuery);

				desc.Query = D3D11_QUERY_TIMESTAMP;
				device->CreateQuery(&desc, &timer.startQuery);
				device->CreateQuery(&desc, &timer.endQuery);

				gpuTimers[funcStr] = std::move(timer);
			}

			ShaderLock();
			context->Begin(gpuTimers[funcStr].disjointQuery.Get());
			context->End(gpuTimers[funcStr].startQuery.Get());
			ShaderUnlock();
		}
		else
		{
			double tick = PerformanceCheckTick ? (double)(RE::GetSecondsSinceLastFrame() * 1000) : (double)(TimeTick60 * 1000);
			ShaderLock();
			context->Flush();
			context->End(gpuTimers[funcStr].endQuery.Get());
			context->End(gpuTimers[funcStr].disjointQuery.Get());
			ShaderUnlock();

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData = {};
			ShaderLock();
			while (context->GetData(gpuTimers[funcStr].disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) != S_OK);
			ShaderUnlock();

			if (disjointData.Disjoint)
				return;

			UINT64 startTime = 0, endTime = 0;
			ShaderLock();
			while (context->GetData(gpuTimers[funcStr].startQuery.Get(), &startTime, sizeof(startTime), 0) != S_OK);
			while (context->GetData(gpuTimers[funcStr].endQuery.Get(), &endTime, sizeof(endTime), 0) != S_OK);
			ShaderUnlock();

			const double duration_ms = (double)(endTime - startTime) / disjointData.Frequency * 1000.0;

			if (isAverage) {
				funcAverageArgs[funcStr] += duration_ms;
				funcAverageCount[funcStr]++;

				if (funcAverageCount[funcStr] >= 60) {
					const double average = funcAverageArgs[funcStr] / funcAverageCount[funcStr];
					logger::info("{} average time: {:.6f}ms{}=> {:.6f}%", funcStr, average,
								 funcAverageArgs[funcStr] > 0 ? (std::string(" with average count ") + std::to_string(funcAverageArgs[funcStr] / funcAverageCount[funcStr]) + " ") : " ",
								 average / tick * 100);
					if (PerformanceCheckConsolePrint) {
						const auto Console = RE::ConsoleLog::GetSingleton();
						if (Console)
							Console->Print("%s average time: %lldms%s=> %.6f%%", funcStr.c_str(), average,
										   funcAverageArgs[funcStr] > 0 ? (std::string(" with average count ") + std::to_string(funcAverageArgs[funcStr] / funcAverageCount[funcStr]) + " ").c_str() : " ",
										   average / tick * 100);
					}
					funcAverageArgs[funcStr] = 0;
					funcAverageCount[funcStr] = 0;
				}
			}
			else {
				logger::info("{} time: {:.6f}ms{}=> {:.6f}%", funcStr, duration_ms,
							 args > 0 ? (std::string(" with count ") + std::to_string(args) + " ") : " ",
							 duration_ms / tick * 100
				);
				if (PerformanceCheckConsolePrint) {
					const auto Console = RE::ConsoleLog::GetSingleton();
					if (Console)
						Console->Print("%s time: %lld ms%s=> %.6f%%", funcStr.c_str(), duration_ms,
									   args > 0 ? (std::string(" with count ") + std::to_string(args) + " ").c_str() : " ",
									   duration_ms / tick * 100);
				}
			}
		}
	}

	ObjectNormalMapUpdater::WaitForGPU::WaitForGPU(ID3D11Device* a_device, ID3D11DeviceContext* a_context, bool a_secondGPUNoWait)
		: device(a_device), context(a_context), secondGPUNoWait(a_secondGPUNoWait)
	{
		if (!device || !context)
			return;

		isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		if (isSecondGPU && secondGPUNoWait)
			return;

		D3D11_QUERY_DESC queryDesc = {};
		queryDesc.Query = D3D11_QUERY_EVENT;
		queryDesc.MiscFlags = 0;

		HRESULT hr = device->CreateQuery(&queryDesc, &query);
		if (FAILED(hr)) {
			return;
		}
		ShaderLock();
		context->Flush();
		context->End(query.Get());
		ShaderUnlock();
		logger::trace("Wait for Renderer...");
	}

	void ObjectNormalMapUpdater::WaitForGPU::Wait()
	{
		if (!device || !context || !query)
			return;
		if (isSecondGPU && secondGPUNoWait)
			return;
		HRESULT hr;
		while (true) {
			ShaderLock();
			hr = context->GetData(query.Get(), nullptr, 0, 0);
			ShaderUnlock();
			if (FAILED(hr))
				break;
			if (hr == S_OK)
				break;
			std::this_thread::sleep_for(waitSleepTime);
		}
		logger::trace("Wait for Renderer done");
		return;
	}

	void ObjectNormalMapUpdater::WaitForFreeVram()
	{
		if (IsBeforeTaskReleased())
			return;
		logger::debug("Wait for vram free...");
		while (!IsBeforeTaskReleased())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		logger::debug("Wait for vram free done");
	}
}
