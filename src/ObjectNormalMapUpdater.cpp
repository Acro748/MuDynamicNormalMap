#include "ObjectNormalMapUpdater.h"
#include "bc7enc.h"

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
				else
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
			Shader::ShaderManager::GetSingleton().GetComputeShader(GetDevice(), MergeTextureShaderName.data());
			Shader::ShaderManager::GetSingleton().GetComputeShader(GetDevice(), GenerateMipsShaderName.data());
		}
	}

	void ObjectNormalMapUpdater::LoadCacheResource(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet& a_updateSet, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results)
	{
		//hash update with geo hash + texture hash
		for (auto& update : a_updateSet) {
			const auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
				return geosInfo.geometry == update.first;
											});
			if (found == a_data->geometries.end())
			{
				logger::error("{}::{:x} : Geometry {} not found in data", __func__, a_actorID, update.second.geometryName);
				a_updateSet.unsafe_erase(update.first);
				continue;
			}
			found->hash = GetHash(update.second, found->hash);
		}

		//find texture from cache
		std::vector<std::pair<RE::BSGeometry*, UpdateTextureSet>> sortUpdateSet(a_updateSet.begin(), a_updateSet.end());
		std::sort(sortUpdateSet.begin(), sortUpdateSet.end(), [&](std::pair<RE::BSGeometry*, UpdateTextureSet>& a, std::pair<RE::BSGeometry*, UpdateTextureSet>& b) {
			const auto aIt = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
				return geosInfo.geometry == a.first;
			});
			if (aIt == a_data->geometries.end())
				return a.first < b.first;
			const auto bIt = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
				return geosInfo.geometry == b.first;
			});
			if (bIt == a_data->geometries.end())
				return a.first < b.first;
			return aIt->objInfo.vertexCount() > bIt->objInfo.vertexCount();
		});
		std::unordered_set<std::uint64_t> pairedHashesAll;
		for (const auto& update : sortUpdateSet) {
			const auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
				return geosInfo.geometry == update.first;
			});
			if (found == a_data->geometries.end())
			{
				logger::error("{}::{:x} : Geometry {} not found in data", __func__, a_actorID, update.second.geometryName);
				continue;
			}
			if (pairedHashesAll.find(found->hash) != pairedHashesAll.end())
				continue;

			TextureResourcePtr textureResource = nullptr;
			bool diskCache = false;
			NormalMapStore::GetSingleton().GetResource(found->hash, textureResource, diskCache);
			if (textureResource)
			{
				std::unordered_set<std::uint64_t> pairedHashes;
				if (!std::ranges::all_of(a_updateSet, [&](auto& updateAlt) {
					if (update.first == updateAlt.first)
						return true;
					if (update.second.textureName != updateAlt.second.textureName)
						return true;
					const auto foundAlt = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
						return geosInfo.geometry == updateAlt.first;
					});
					if (foundAlt == a_data->geometries.end())
					{
						logger::error("{}::{:x} : Geometry {} not found in data", __func__, a_actorID, update.second.geometryName);
						return true;
					}
					pairedHashes.insert(found->hash);
					pairedHashes.insert(foundAlt->hash);
					return NormalMapStore::GetSingleton().IsPairHashes(found->hash, foundAlt->hash);
				}))
				{
					NormalMapStore::GetSingleton().InitHashPair(found->hash);
					logger::info("{}::{:x}::{} : Found exist resource from {} ({:x}), but lack of components. so skip", __func__, a_actorID, 
								 update.second.geometryName, (diskCache ? "disk cache" : "GPU"), found->hash);
					continue;
				}

				logger::info("{}::{:x}::{} : Found exist resource from {} ({:x})", __func__, a_actorID, update.second.geometryName, (diskCache ? "disk cache" : "GPU"), found->hash);
				NormalMapResult newNormalMapResult;
				newNormalMapResult.slot = update.second.slot;
				newNormalMapResult.geometry = update.first;
				newNormalMapResult.vertexCount = found->objInfo.vertexCount();
				newNormalMapResult.geoName = update.second.geometryName;
				newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
				newNormalMapResult.textureName = update.second.textureName;
				newNormalMapResult.texture = textureResource;
				newNormalMapResult.hash = found->hash;
				newNormalMapResult.existResource = true;
				newNormalMapResult.diskCache = diskCache;
				results.push_back(newNormalMapResult);

				TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
				newResourceData->textureName = update.second.textureName;
				resourceDatas.push_back(newResourceData);

				a_updateSet.unsafe_erase(update.first);

				pairedHashesAll.insert(pairedHashes.begin(), pairedHashes.end());
			}
		}

		//shere resource and use GPU resource instead of disk cache if multiple cache is loaded on the same texture name
		for (auto& result : results) {
			if (!result.existResource)
				continue;
			auto resultFound = std::find_if(results.begin(), results.end(), [&](NormalMapResult& r) {
				return result.geometry != r.geometry && result.textureName == r.textureName && result.existResource == r.existResource;
											});
			if (resultFound == results.end())
				continue;

			NormalMapResult* srcResult = nullptr;
			NormalMapResult* dstResult = nullptr;
			if (result.diskCache == resultFound->diskCache)
			{
				if (result.vertexCount >= resultFound->vertexCount)
				{
					srcResult = &result;
					dstResult = &*resultFound;
				}
				else
				{
					srcResult = &*resultFound;
					dstResult = &result;
				}
			}
			else
			{
				if (!result.diskCache)
				{
					srcResult = &result;
					dstResult = &*resultFound;
				}
				else
				{
					srcResult = &*resultFound;
					dstResult = &result;
				}
			}
			dstResult->diskCache = srcResult->diskCache;
			dstResult->texture = srcResult->texture;
			logger::info("{}::{:x}::{} : Use share resource {:x}", __func__, a_actorID, dstResult->geoName, srcResult->hash);
		}

		//share resource if textureName is the same
		auto updateSet_ = a_updateSet;
		for (auto& update : updateSet_) {
			const auto found = std::find_if(a_data->geometries.begin(), a_data->geometries.end(), [&](GeometryData::GeometriesInfo& geosInfo) {
				return geosInfo.geometry == update.first;
											});
			if (found == a_data->geometries.end())
			{
				continue;
			}

			if (std::find_if(results.begin(), results.end(), [&](NormalMapResult& result) { return update.first == result.geometry; }) != results.end())
				continue;

			const auto resultFound = std::find_if(results.begin(), results.end(), [&](NormalMapResult& result) {
				return update.first != result.geometry && update.second.textureName == result.textureName && result.existResource;
			});
			if (resultFound == results.end())
				continue;

			logger::info("{}::{:x}::{} : Use share resource {:x}", __func__, a_actorID, update.second.geometryName, resultFound->hash);

			NormalMapResult newNormalMapResult;
			newNormalMapResult.slot = update.second.slot;
			newNormalMapResult.geometry = update.first;
			newNormalMapResult.vertexCount = found->objInfo.vertexCount();
			newNormalMapResult.geoName = update.second.geometryName;
			newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
			newNormalMapResult.textureName = update.second.textureName;
			newNormalMapResult.texture = resultFound->texture;
			newNormalMapResult.hash = found->hash;
			newNormalMapResult.existResource = true;
			newNormalMapResult.diskCache = resultFound->diskCache;
			results.push_back(newNormalMapResult);

			TextureResourceDataPtr newResourceData = std::make_shared<TextureResourceData>();
			newResourceData->textureName = update.second.textureName;
			resourceDatas.push_back(newResourceData);

			mergedTextureGeometries.insert(update.first);
			a_updateSet.unsafe_erase(update.first);
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
		Shader::ShaderLocker sl(context);

		std::unordered_set<RE::BSGeometry*> mergedTextureGeometries;
		concurrency::concurrent_vector<TextureResourceDataPtr> resourceDatas;
		LoadCacheResource(a_actorID, a_data, a_updateSet, mergedTextureGeometries, resourceDatas, results);

		CRITICAL_SECTION resultLock;
		InitializeCriticalSectionAndSpinCount(&resultLock, 4000);
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
			newResourceData->geometry = update.first;
			newResourceData->textureName = update.second.textureName;

			D3D11_TEXTURE2D_DESC srcStagingDesc = {}, detailStagingDesc = {}, overlayStagingDesc = {}, maskStagingDesc = {}, dstDesc = {};

			if (!update.second.srcTexturePath.empty())
			{
				if (!IsDetailNormalMap(update.second.srcTexturePath))
				{
					logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.srcTexturePath);

					if (LoadTextureCPU(device, context, update.second.srcTexturePath, srcStagingDesc, newResourceData->srcTexture2D))
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

				if (LoadTextureCPU(device, context, update.second.detailTexturePath, detailStagingDesc, newResourceData->detailTexture2D))
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

			dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstDesc.Usage = D3D11_USAGE_STAGING;
			dstDesc.BindFlags = 0;
			dstDesc.MiscFlags = 0;
			dstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
			dstDesc.ArraySize = 1;
			dstDesc.SampleDesc.Count = 1;
			if (Config::GetSingleton().GetUseMipMap())
			{
				UINT widthMips = log2(dstDesc.Width) + 1;
				UINT heightMips = log2(dstDesc.Width) + 1;
				dstDesc.MipLevels = std::max(widthMips, heightMips);
			}
			else
			{
				dstDesc.MipLevels = 1;
			}

			Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D;
			hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
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

			D3D11_MAPPED_SUBRESOURCE dstMappedResource, srcMappedResource, detailMappedResource, overlayMappedResource, maskMappedResource;
			std::uint8_t* srcData = nullptr;
			std::uint8_t* detailData = nullptr;
			std::uint8_t* overlayData = nullptr;
			std::uint8_t* maskData = nullptr;

			sl.Lock();
			hr = context->Map(dstTexture2D.Get(), 0, D3D11_MAP_WRITE, 0, &dstMappedResource);
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
			sl.Unlock();

			const bool hasSrcData = (srcData != nullptr);
			const bool hasDetailData = (detailData != nullptr);
			const bool hasOverlayData = (overlayData != nullptr);
			const bool hasMaskData = (maskData != nullptr);

			if (Config::GetSingleton().GetUpdateNormalMapTime1())
				PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, true, false);

			std::uint32_t totalTris = objInfo.indicesCount() / 3;

			const UINT width = dstDesc.Width;
			const UINT height = dstDesc.Height;
			std::uint8_t* dstData = reinterpret_cast<std::uint8_t*>(dstMappedResource.pData);

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

							std::uint8_t* rowData = dstData + y * dstMappedResource.RowPitch;
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

			sl.Lock();
			context->Unmap(dstTexture2D.Get(), 0);
			if (hasSrcData)
				context->Unmap(newResourceData->srcTexture2D.Get(), 0);
			if (hasDetailData)
				context->Unmap(newResourceData->detailTexture2D.Get(), 0);
			if (hasOverlayData)
				context->Unmap(newResourceData->overlayTexture2D.Get(), 0);
			if (hasMaskData)
				context->Unmap(newResourceData->maskTexture2D.Get(), 0);
			sl.Unlock();

			NormalMapResult newNormalMapResult;
			newNormalMapResult.slot = update.second.slot;
			newNormalMapResult.geometry = update.first;
			newNormalMapResult.vertexCount = objInfo.vertexCount();
			newNormalMapResult.geoName = update.second.geometryName;
			newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
			newNormalMapResult.textureName = update.second.textureName;
			newNormalMapResult.texture = std::make_shared<TextureResource>();
			newNormalMapResult.texture->normalmapTexture2D = dstTexture2D;
			newNormalMapResult.texture->normalmapShaderResourceView = nullptr;
			newNormalMapResult.hash = found->hash;

			EnterCriticalSection(&resultLock);
			results.push_back(newNormalMapResult);
			resourceDatas.push_back(newResourceData);
			LeaveCriticalSection(&resultLock);
		}
		PostProcessing(device, context, resourceDatas, results, mergedTextureGeometries);
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

		Shader::ShaderLocker sl(context);

		const GeometryResourceDataPtr geoData = GetGeometryResourceData(a_actorID);
		if (!geoData)
		{
			logger::error("{}::{:x} : GeometryResourceData not found for actor", _func_, a_actorID);
			return results;
		}
		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_updateSet.size());

		std::unordered_set<RE::BSGeometry*> mergedTextureGeometries;
		concurrency::concurrent_vector<TextureResourceDataPtr> resourceDatas;
		LoadCacheResource(a_actorID, a_data, a_updateSet, mergedTextureGeometries, resourceDatas, results);

		CRITICAL_SECTION resultLock;
		InitializeCriticalSectionAndSpinCount(&resultLock, 4000);
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
				newResourceData->geometry = update.first;
				newResourceData->textureName = update.second.textureName;

				D3D11_TEXTURE2D_DESC srcDesc = {}, detailDesc = {}, overlayDesc = {}, maskDesc = {}, dstDesc = {};
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

				dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstDesc.Usage = D3D11_USAGE_DEFAULT;
				dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
				dstDesc.ArraySize = 1;
				dstDesc.MiscFlags = 0;
				dstDesc.CPUAccessFlags = 0;
				dstDesc.SampleDesc.Count = 1;
				if (Config::GetSingleton().GetUseMipMap())
				{
					UINT widthMips = log2(dstDesc.Width) + 1;
					UINT heightMips = log2(dstDesc.Width) + 1;
					dstDesc.MipLevels = std::max(widthMips, heightMips);
				}
				else
				{
					dstDesc.MipLevels = 1;
				}

				Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D = nullptr;
				hr = device->CreateTexture2D(&dstDesc, nullptr, &dstTexture2D);
				if (FAILED(hr))
				{
					logger::error("{}::{:x}::{} : Failed to create dst texture 2d ({})", _func_, a_actorID, update.second.geometryName, hr);
					return;
				}

				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstShaderResourceView = nullptr;
				dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				dstShaderResourceViewDesc.Texture2D.MipLevels = dstDesc.MipLevels;
				dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
				hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstShaderResourceView);
				if (FAILED(hr))
				{
					logger::error("{}::{:x}::{} : Failed to create dst shader resource view ({})", _func_, a_actorID, update.second.geometryName, hr);
					return;
				}

				D3D11_UNORDERED_ACCESS_VIEW_DESC dstUnorderedViewDesc = {};
				dstUnorderedViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				dstUnorderedViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				dstUnorderedViewDesc.Texture2D.MipSlice = 0;
				hr = device->CreateUnorderedAccessView(dstTexture2D.Get(), &dstUnorderedViewDesc, &newResourceData->dstUnorderedAccessView);
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
				if (sl.IsSecondGPU() || isNoSplitGPU)
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
					sl.Lock();
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
					context->CSSetUnorderedAccessViews(0, 1, newResourceData->dstUnorderedAccessView.GetAddressOf(), nullptr);
					context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
					context->Dispatch(dispatch, 1, 1);
					sb.Revert(context);
					sl.Unlock();

					if (Config::GetSingleton().GetUpdateNormalMapTime2())
						GPUPerformanceLog(device, context, std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName, true, false);

					newResourceData->constBuffers.push_back(constBuffer);
				}
				else
				{
					const std::uint32_t subResolution = 4096 / (1u << divideTaskQ);
					const std::uint32_t subSize = (width / subResolution) * (height / subResolution);

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
							sl.Lock();
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
							context->CSSetUnorderedAccessViews(0, 1, newResourceData->dstUnorderedAccessView.GetAddressOf(), nullptr);
							context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
							context->Dispatch(dispatch, 1, 1);
							sb.Revert(context);
							sl.Unlock();

							if (Config::GetSingleton().GetUpdateNormalMapTime2())
								GPUPerformanceLog(device, context, std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName + "::" + std::to_string(subIndex), true, false);
						}));
						newResourceData->constBuffers.push_back(constBuffer);
					}
					for (auto& task : gpuTasks) {
						task.get();
					}
				}

				NormalMapResult newNormalMapResult;
				newNormalMapResult.slot = update.second.slot;
				newNormalMapResult.geometry = update.first;
				newNormalMapResult.vertexCount = objInfo.vertexCount();
				newNormalMapResult.geoName = update.second.geometryName;
				newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
				newNormalMapResult.textureName = update.second.textureName;
				newNormalMapResult.texture = std::make_shared<TextureResource>();
				newNormalMapResult.texture->normalmapTexture2D = dstTexture2D;
				newNormalMapResult.texture->normalmapShaderResourceView = dstShaderResourceView;
				newNormalMapResult.hash = found->hash;

				EnterCriticalSection(&resultLock);
				results.push_back(newNormalMapResult);
				resourceDatas.push_back(newResourceData);
				LeaveCriticalSection(&resultLock);
			};
			updateTasks.push_back(updateThreads->submitAsync([updateTaskFunc] {
				updateTaskFunc();
			}));
		}
		for (auto& updateTask : updateTasks) {
			updateTask.get();
		}
		if (Config::GetSingleton().GetGPUForceSync())
			WaitForGPU(device, context).Wait();
		PostProcessingGPU(device, context, resourceDatas, results, mergedTextureGeometries);
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
			if (!Shader::TextureLoadManager::GetSingleton().GetTextureFromFile(device, context, filePath, tmpTexDesc, tmpSrvDesc, DXGI_FORMAT_UNKNOWN, false, texture))
				return false;
		}
		else
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTexture2D(filePath, tmpTexDesc, tmpSrvDesc, DXGI_FORMAT_UNKNOWN, false, texture))
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

	bool ObjectNormalMapUpdater::LoadTextureCPU(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
	{
		if (!device || !context || filePath.empty())
			return false;

		HRESULT hr;
		D3D11_SHADER_RESOURCE_VIEW_DESC tmpSrvDesc;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		if (Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context))
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTextureFromFile(device, context, filePath, texDesc, tmpSrvDesc, DXGI_FORMAT_R8G8B8A8_UNORM, true, texture))
				return false;
		}
		else
		{
			if (!Shader::TextureLoadManager::GetSingleton().GetTexture2D(filePath, texDesc, tmpSrvDesc, DXGI_FORMAT_R8G8B8A8_UNORM, true, texture))
				return false;
		}
		texture->GetDesc(&texDesc);
		if (texDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)
		{
			output = texture;
			return true;
		}
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		texDesc.Usage = D3D11_USAGE_STAGING;
		texDesc.MiscFlags = 0;
		texDesc.BindFlags = 0;
		hr = device->CreateTexture2D(&texDesc, nullptr, output.GetAddressOf());
		if (FAILED(hr))
		{
			logger::error("Failed to create texture 2d ({})", hr);
			return false;
		}
		return CopySubresourceRegion(device, context, output.Get(), texture.Get(), 0, 0);
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

		Shader::ShaderLocker sl(context);

		if (sl.IsSecondGPU() || isNoSplitGPU || (dstWidth <= 256 && dstHeight <= 256))
		{
			sl.Lock();
			context->CopySubresourceRegion(dstTexture, dstMipMapLevel, 0, 0, 0, srcTexture, srcMipMapLevel, nullptr);
			sl.Unlock();
			return true;
		}

		const std::uint32_t subResolution = 4096 / (1u << divideTaskQ);
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

					sl.Lock();
					context->CopySubresourceRegion(
						dstTexture, dstMipMapLevel,
						box.left, box.top, 0,
						srcTexture, srcMipMapLevel,
						&box);
					sl.Unlock();

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

	void ObjectNormalMapUpdater::PostProcessing(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		//merge texture
		{
			bool merged = false;
			auto results_ = results;
			std::sort(results_.begin(), results_.end(), [](NormalMapResult& a, NormalMapResult& b) {
				return a.vertexCount > b.vertexCount;
			});
			for (auto& dst : results_)
			{
				if (mergedTextureGeometries.find(dst.geometry) != mergedTextureGeometries.end())
					continue;

				for (auto& src : results)
				{
					if (src.geometry == dst.geometry)
						continue;
					if (mergedTextureGeometries.find(src.geometry) != mergedTextureGeometries.end())
						continue;
					if (src.textureName != dst.textureName)
						continue;
					if (src.vertexCount > dst.vertexCount)
						continue;

					auto srcResource = std::find_if(resourceDatas.begin(), resourceDatas.end(), [&](TextureResourceDataPtr& resource) { return resource->geometry == src.geometry; });
					if (srcResource == resourceDatas.end())
						continue;
					logger::info("{} : Merge texture {} into {}...", src.textureName, src.geoName, dst.geoName);
					bool mergeResult = false;
					mergeResult = MergeTexture(device, context, *srcResource,
												  dst.texture->normalmapTexture2D.Get(), src.texture->normalmapTexture2D.Get());
					if (mergeResult)
					{
						src.texture = dst.texture;
						mergedTextureGeometries.insert(src.geometry);
						merged = true;
						NormalMapStore::GetSingleton().AddHashPair(dst.hash, src.hash);
					}
				}
			}
		}

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		std::vector<std::future<void>> updateTasks;
		concurrency::concurrent_unordered_set<std::uint32_t> failedCopyResources;
		for (std::uint32_t i = 0; i < results.size(); i++)
		{
			updateTasks.push_back(updateThreads->submitAsync([&, i]() {
				if (results[i].existResource)
					return;
				if (mergedTextureGeometries.find(results[i].geometry) != mergedTextureGeometries.end())
					return;
				if (Config::GetSingleton().GetUseMipMap())
				{
					GenerateMips(device, context, resourceDatas[i], results[i].texture->normalmapTexture2D.Get());
				}
				CompressTexture(device, context, resourceDatas[i], results[i].texture->normalmapTexture2D);
				if (!isSecondGPU)
				{
					D3D11_TEXTURE2D_DESC desc;
					results[i].texture->normalmapTexture2D->GetDesc(&desc);
					HRESULT hr;
					if (!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
					{
						resourceDatas[i]->stagingTexture2D = results[i].texture->normalmapTexture2D;
						desc.Usage = D3D11_USAGE_DEFAULT;
						desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
						desc.CPUAccessFlags = 0;
						desc.MiscFlags = 0;
						hr = device->CreateTexture2D(&desc, nullptr, results[i].texture->normalmapTexture2D.ReleaseAndGetAddressOf());
						if (FAILED(hr)) {
							logger::error("{} : Failed to create Texture2D ({})", resourceDatas[i]->textureName, hr);
							failedCopyResources.insert(i);
							return;
						}
						for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
						{
							CopySubresourceRegion(device, context, results[i].texture->normalmapTexture2D.Get(), resourceDatas[i]->stagingTexture2D.Get(), mipLevel, mipLevel);
						}
					}
					D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
					srvDesc.Format = desc.Format;
					srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Texture2D.MipLevels = desc.MipLevels;
					srvDesc.Texture2D.MostDetailedMip = 0;
					hr = device->CreateShaderResourceView(results[i].texture->normalmapTexture2D.Get(), &srvDesc, results[i].texture->normalmapShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr)) {
						logger::error("{} : Failed to create ShaderResourceView ({})", resourceDatas[i]->textureName, hr);
						failedCopyResources.insert(i);
						return;
					}

					resourceDatas[i]->GetQuery(device, context);
					ResourceDataMapLock.lock_shared();
					ResourceDataMap.push_back(resourceDatas[i]);
					ResourceDataMapLock.unlock_shared();
					logger::info("{} : normalmap created", results[i].textureName, results[i].geoName);
					NormalMapStore::GetSingleton().AddResource(results[i].hash, results[i].texture);
				}
				else
				{
					if (CopyResourceSecondToMain(resourceDatas[i], results[i].texture->normalmapTexture2D, results[i].texture->normalmapShaderResourceView))
					{
						logger::info("{} : normalmap created", results[i].textureName, results[i].geoName);
						NormalMapStore::GetSingleton().AddResource(results[i].hash, results[i].texture);
					}
					else
						failedCopyResources.insert(i);
				}
			}));
		}
		for (auto& task : updateTasks) {
			task.get();
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
	void ObjectNormalMapUpdater::PostProcessingGPU(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries)
	{
		//merge texture
		{
			bool merged = false;
			auto results_ = results;
			std::sort(results_.begin(), results_.end(), [](NormalMapResult& a, NormalMapResult& b) {
				return a.vertexCount > b.vertexCount;
			});
			for (auto& dst : results_)
			{
				if (mergedTextureGeometries.find(dst.geometry) != mergedTextureGeometries.end())
					continue;

				for (auto& src : results)
				{
					if (src.geometry == dst.geometry)
						continue;
					if (mergedTextureGeometries.find(src.geometry) != mergedTextureGeometries.end())
						continue;
					if (src.textureName != dst.textureName)
						continue;
					if (src.vertexCount > dst.vertexCount)
						continue;

					auto resourceIt = std::find_if(resourceDatas.begin(), resourceDatas.end(), [&](TextureResourceDataPtr& resource) { return resource->geometry == src.geometry; });
					logger::info("{} : Merge texture {} into {}...", src.textureName, src.geoName, dst.geoName);
					bool mergeResult = false;
					mergeResult = MergeTextureGPU(device, context, *resourceIt, 
												  dst.texture->normalmapShaderResourceView.Get(), dst.texture->normalmapTexture2D.Get(),
												  src.texture->normalmapShaderResourceView.Get(), src.texture->normalmapTexture2D.Get());
					if (mergeResult)
					{
						src.texture = dst.texture;
						mergedTextureGeometries.insert(src.geometry);
						merged = true;
						NormalMapStore::GetSingleton().AddHashPair(dst.hash, src.hash);
					}
				}
			}
			if (merged && Config::GetSingleton().GetGPUForceSync())
				WaitForGPU(device, context).Wait();
		}

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		std::vector<std::future<void>> updateTasks;
		concurrency::concurrent_unordered_set<std::uint32_t> failedCopyResources;
		for (std::uint32_t i = 0; i < results.size(); i++)
		{
			updateTasks.push_back(updateThreads->submitAsync([&, i]() {
				if (results[i].existResource)
					return;
				if (mergedTextureGeometries.find(results[i].geometry) != mergedTextureGeometries.end())
					return;
				if (Config::GetSingleton().GetUseMipMap())
				{
					GenerateMipsGPU(device, context, resourceDatas[i], results[i].texture->normalmapShaderResourceView.Get(), results[i].texture->normalmapTexture2D.Get());
				}
				CompressTexture(device, context, resourceDatas[i], results[i].texture->normalmapTexture2D);
				if (!isSecondGPU)
				{
					D3D11_TEXTURE2D_DESC desc;
					results[i].texture->normalmapTexture2D->GetDesc(&desc);
					HRESULT hr;
					if (!(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE))
					{
						resourceDatas[i]->stagingTexture2D = results[i].texture->normalmapTexture2D;
						desc.Usage = D3D11_USAGE_DEFAULT;
						desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
						desc.CPUAccessFlags = 0;
						desc.MiscFlags = 0;
						hr = device->CreateTexture2D(&desc, nullptr, results[i].texture->normalmapTexture2D.ReleaseAndGetAddressOf());
						if (FAILED(hr)) {
							logger::error("{} : Failed to create Texture2D ({})", resourceDatas[i]->textureName, hr);
							failedCopyResources.insert(i);
							return;
						}
						for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
						{
							CopySubresourceRegion(device, context, results[i].texture->normalmapTexture2D.Get(), resourceDatas[i]->stagingTexture2D.Get(), mipLevel, mipLevel);
						}
					}
					D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
					srvDesc.Format = desc.Format;
					srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Texture2D.MipLevels = desc.MipLevels;
					srvDesc.Texture2D.MostDetailedMip = 0;
					hr = device->CreateShaderResourceView(results[i].texture->normalmapTexture2D.Get(), &srvDesc, results[i].texture->normalmapShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr)) {
						logger::error("{} : Failed to create ShaderResourceView ({})", resourceDatas[i]->textureName, hr);
						failedCopyResources.insert(i);
						return;
					}

					resourceDatas[i]->GetQuery(device, context);
					ResourceDataMapLock.lock_shared();
					ResourceDataMap.push_back(resourceDatas[i]);
					ResourceDataMapLock.unlock_shared();
					logger::info("{} : normalmap created", results[i].textureName, results[i].geoName);
					NormalMapStore::GetSingleton().AddResource(results[i].hash, results[i].texture);
				}
				else
				{
					if (CopyResourceSecondToMain(resourceDatas[i], results[i].texture->normalmapTexture2D, results[i].texture->normalmapShaderResourceView))
					{
						logger::info("{} : normalmap created", results[i].textureName, results[i].geoName);
						NormalMapStore::GetSingleton().AddResource(results[i].hash, results[i].texture);
					}
					else
						failedCopyResources.insert(i);
				}
			}));
		}
		for (auto& task : updateTasks) {
			task.get();
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

	bool ObjectNormalMapUpdater::MergeTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& srcResourceData, ID3D11Texture2D* dstTex, ID3D11Texture2D* srcTex)
	{
		if (!device || !context)
			return false;

		logger::info("{}::{} : Merge texture...", __func__, srcResourceData->textureName);

		if (Config::GetSingleton().GetMergeTime())
			PerformanceLog(std::string(__func__) + "::" + srcResourceData->textureName, false, false);

		Shader::ShaderLocker sl(context);

		D3D11_TEXTURE2D_DESC desc;
		dstTex->GetDesc(&desc);

		D3D11_MAPPED_SUBRESOURCE dstMappedResource, srcMappedResource;
		sl.Lock();
		context->Map(dstTex, 0, D3D11_MAP_READ_WRITE, 0, &dstMappedResource);
		context->Map(srcTex, 0, D3D11_MAP_READ, 0, &srcMappedResource);
		sl.Unlock();

		const UINT width = desc.Width;
		const UINT height = desc.Height;

		std::uint8_t* dst_pData = reinterpret_cast<std::uint8_t*>(dstMappedResource.pData);
		std::uint8_t* src_pData = reinterpret_cast<std::uint8_t*>(srcMappedResource.pData);
		std::vector<std::future<void>> processes;
		const std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(width), processingThreads->GetThreads() * 4));
		const std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(height), processingThreads->GetThreads() * 4));
		const std::size_t unitX = (std::size_t(width) + subX - 1) / subX;
		const std::size_t unitY = (std::size_t(height) + subY - 1) / subY;
		for (std::size_t px = 0; px < subX; px++) {
			for (std::size_t py = 0; py < subY; py++) {
				std::size_t beginX = px * unitX;
				std::size_t endX = std::min(beginX + unitX, std::size_t(width));
				std::size_t beginY = py * unitY;
				std::size_t endY = std::min(beginY + unitY, std::size_t(height));
				processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
					for (UINT y = beginY; y < endY; y++) {
						std::uint8_t* dstRowData = dst_pData + y * dstMappedResource.RowPitch;
						std::uint8_t* srcRowData = src_pData + y * srcMappedResource.RowPitch;
						for (UINT x = beginX; x < endX; x++)
						{
							std::uint32_t* dstPixel = reinterpret_cast<uint32_t*>(dstRowData + x * 4);
							std::uint32_t* srcPixel = reinterpret_cast<uint32_t*>(srcRowData + x * 4);
							RGBA dstPixelColor, srcPixelColor;
							dstPixelColor.SetReverse(*dstPixel);
							srcPixelColor.SetReverse(*srcPixel);
							if (dstPixelColor.a < 1.0f && srcPixelColor.a < 1.0f)
								continue;

							RGBA dstColor = RGBA::lerp(srcPixelColor, dstPixelColor, dstPixelColor.a);
							dstColor.a = 1.0f;
							*dstPixel = dstColor.GetReverse();
						}
					}
				}));
			}
		}
		for (auto& process : processes)
		{
			process.get();
		}

		sl.Lock();
		context->Unmap(dstTex, 0);
		context->Unmap(srcTex, 0);
		sl.Unlock();

		if (Config::GetSingleton().GetMergeTime())
			PerformanceLog(std::string(__func__) + "::" + srcResourceData->textureName, true, false);
		logger::debug("{}::{} : Merge texture done", __func__, srcResourceData->textureName);
		return true;
	}
	bool ObjectNormalMapUpdater::MergeTextureGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* dstSrv, ID3D11Texture2D* dstTex, ID3D11ShaderResourceView* srcSrv, ID3D11Texture2D* srcTex)
	{
		if (!device || !context || !srcTex || !srcSrv || !dstTex || !dstSrv)
			return false;

		const auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(device, MergeTextureShaderName.data());
		if (!shader)
			return false;

		const std::string _func_ = __func__;
		logger::info("{}::{} : Merge texture...", _func_, resourceData->textureName);

		Shader::ShaderLocker sl(context);

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

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		hr = device->CreateUnorderedAccessView(dstTex, &uavDesc, &resourceData->mergeTextureData.uav);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}

		if (sl.IsSecondGPU() || isNoSplitGPU)
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

			if (Config::GetSingleton().GetMergeTime())
				GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
								  , false, false);

			ShaderBackup sb;
			sl.Lock();
			sb.Backup(context);
			context->CSSetShader(shader.Get(), nullptr, 0);
			context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
			context->CSSetShaderResources(0, 1, &srcSrv);
			context->CSSetUnorderedAccessViews(0, 1, resourceData->mergeTextureData.uav.GetAddressOf(), nullptr);
			context->Dispatch(dispatch.x, dispatch.y, 1);
			sb.Revert(context);
			sl.Unlock();

			if (Config::GetSingleton().GetMergeTime())
				GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
								  , true, false);

			resourceData->mergeTextureData.constBuffers.push_back(constBuffer);
		}
		else
		{
			const std::uint32_t subResolution = 4096 / (1u << divideTaskQ);
			const DirectX::XMUINT2 dispatch = { (std::min(width, subResolution) + 8 - 1) / 8, (std::min(height, subResolution) + 8 - 1) / 8 };
			const std::uint32_t subXSize = std::max(1u, width / subResolution);
			const std::uint32_t subYSize = std::max(1u, height / subResolution);

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
						if (Config::GetSingleton().GetMergeTime())
							GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
											  + "::" + std::to_string(subX) + "|" + std::to_string(subY), false, false);

						ShaderBackup sb;
						sl.Lock();
						sb.Backup(context);
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						context->CSSetShaderResources(0, 1, &srcSrv);
						context->CSSetUnorderedAccessViews(0, 1, resourceData->mergeTextureData.uav.GetAddressOf(), nullptr);
						context->Dispatch(dispatch.x, dispatch.y, 1);
						sb.Revert(context);
						sl.Unlock();

						if (Config::GetSingleton().GetMergeTime())
							GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
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
		logger::debug("{}::{} : Merge texture done", _func_, resourceData->textureName);
		return true;
	}

	bool ObjectNormalMapUpdater::GenerateMips(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11Texture2D* texInOut)
	{
		if (!device || !context || !texInOut)
			return false;

		Shader::ShaderLocker sl(context);

		HRESULT hr;
		const std::string _func_ = __func__;

		logger::info("{}::{} : Generate Mips...", _func_, resourceData->textureName);

		if (Config::GetSingleton().GetGenerateMipsTime())
			PerformanceLog(_func_ + "::" + resourceData->textureName
							  , false, false);

		resourceData->generateMipsData.texture2D = texInOut;

		D3D11_TEXTURE2D_DESC desc;
		resourceData->generateMipsData.texture2D->GetDesc(&desc);

		std::vector<D3D11_MAPPED_SUBRESOURCE> mappedResource(desc.MipLevels);
		sl.Lock();
		for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
		{
			context->Map(resourceData->generateMipsData.texture2D.Get(), mipLevel, D3D11_MAP_READ_WRITE, 0, &mappedResource[mipLevel]);
		}
		sl.Unlock();

		const UINT width = desc.Width;
		const UINT height = desc.Height; 

		//mipLevel 0
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
		for (std::uint8_t i = 0; i < 2; i++)
		{
			std::vector<std::future<void>> processes;
			const std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(width), processingThreads->GetThreads() * 16));
			const std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(height), processingThreads->GetThreads() * 16));
			const std::size_t unitX = (std::size_t(width) + subX - 1) / subX;
			const std::size_t unitY = (std::size_t(height) + subY - 1) / subY;
			std::uint8_t* pData = reinterpret_cast<std::uint8_t*>(mappedResource[0].pData);
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
							std::uint8_t* rowData = pData + y * mappedResource[0].RowPitch;
							for (UINT x = beginX; x < endX; x++)
							{
								std::uint32_t* pixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
								RGBA color;
								color.SetReverse(*pixel);
								if (color.A() == 0xFF)
									continue;

								RGBA averageColor(0.0f, 0.0f, 0.0f, 0.0f);
								std::uint8_t validCount = 0;
								for (std::uint8_t i = 0; i < 8; i++)
								{
									DirectX::XMINT2 nearCoord = { (INT)x + offsets[i].x, (INT)y + offsets[i].y };
									if (nearCoord.x < 0 || nearCoord.y < 0 ||
										nearCoord.x >= width || nearCoord.y >= height)
										continue;

									std::uint8_t* nearRowData = pData + nearCoord.y * mappedResource[0].RowPitch;
									std::uint32_t* nearPixel = reinterpret_cast<uint32_t*>(nearRowData + nearCoord.x * 4);
									RGBA nearColor;
									nearColor.SetReverse(*nearPixel);
									if (nearColor.A() == 0xFF)
									{
										averageColor += nearColor;
										validCount++;
									}
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
		}

		const DirectX::XMUINT2 sampleOffsets[4] = {
			DirectX::XMUINT2(0u, 0u), // left up
			DirectX::XMUINT2(1u, 0u), // right up
			DirectX::XMUINT2(0u, 1u), // left down
			DirectX::XMUINT2(1u, 1u)  // right down
		};
		for (UINT mipLevel = 1; mipLevel < desc.MipLevels; mipLevel++)
		{
			const UINT mipWidth = std::max(width >> mipLevel, 1u);
			const UINT mipHeight = std::max(height >> mipLevel, 1u);

			const UINT srcMipLevel = mipLevel - 1;
			const UINT srcMipWidth = std::max(width >> srcMipLevel, 1u);
			const UINT srcMipHeight = std::max(height >> srcMipLevel, 1u);

			std::uint8_t* dst_pData = reinterpret_cast<std::uint8_t*>(mappedResource[mipLevel].pData);
			std::uint8_t* src_pData = reinterpret_cast<std::uint8_t*>(mappedResource[srcMipLevel].pData);
			std::vector<std::future<void>> processes;
			const std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(mipWidth), processingThreads->GetThreads() * 8));
			const std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(mipHeight), processingThreads->GetThreads() * 8));
			const std::size_t unitX = (std::size_t(mipWidth) + subX - 1) / subX;
			const std::size_t unitY = (std::size_t(mipHeight) + subY - 1) / subY;
			for (std::size_t px = 0; px < subX; px++) {
				for (std::size_t py = 0; py < subY; py++) {
					std::size_t beginX = px * unitX;
					std::size_t endX = std::min(beginX + unitX, std::size_t(mipWidth));
					std::size_t beginY = py * unitY;
					std::size_t endY = std::min(beginY + unitY, std::size_t(mipHeight));
					processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
						for (UINT y = beginY; y < endY; y++) {
							std::uint8_t* dstRowData = dst_pData + y * mappedResource[mipLevel].RowPitch;
							for (UINT x = beginX; x < endX; x++)
							{
								std::uint32_t* pixel = reinterpret_cast<uint32_t*>(dstRowData + x * 4);
								RGBA averageColor(0.0f, 0.0f, 0.0f, 0.0f);
								std::uint8_t validCount = 0;
#pragma unroll(4)
								for (std::uint8_t i = 0; i < 4; i++)
								{
									DirectX::XMUINT2 srcCoord = { x * 2 + sampleOffsets[i].x, y * 2 + sampleOffsets[i].y };
									if (srcCoord.x >= srcMipWidth || srcCoord.y >= srcMipHeight)
										continue;
									std::uint8_t* srcRowData = src_pData + mappedResource[srcMipLevel].RowPitch;
									std::uint32_t* srcPixel = reinterpret_cast<uint32_t*>(srcRowData + srcCoord.x * 4);
									RGBA srcColor;
									srcColor.SetReverse(*srcPixel);
									if (srcColor.A() == 0xFF)
									{
										averageColor += srcColor;
										validCount++;
									}
								}
								if (validCount == 0)
								{
#pragma unroll(8)
									for (std::uint8_t i = 0; i < 8; i++)
									{
										DirectX::XMINT2 nearCoord = { (INT)x + offsets[i].x, (INT)y + offsets[i].y };
										if (nearCoord.x < 0 || nearCoord.y < 0 ||
											nearCoord.x >= mipWidth || nearCoord.y >= mipHeight)
											continue;
#pragma unroll(4)
										for (std::uint8_t j = 0; j < 4; j++)
										{
											DirectX::XMUINT2 srcCoord = { nearCoord.x * 2 + sampleOffsets[j].x, nearCoord.y * 2 + sampleOffsets[j].y };
											if (srcCoord.x >= srcMipWidth || srcCoord.y >= srcMipHeight)
												continue;
											std::uint8_t* srcRowData = src_pData + srcCoord.y * mappedResource[srcMipLevel].RowPitch;
											std::uint32_t* srcPixel = reinterpret_cast<uint32_t*>(srcRowData + srcCoord.x * 4);
											RGBA srcColor;
											srcColor.SetReverse(*srcPixel);
											if (srcColor.A() == 0xFF)
											{
												averageColor += srcColor;
												validCount++;
											}
										}
									}
								}
								if (validCount == 0)
								{
									*pixel = emptyColor.GetReverse();
									continue;
								}
								RGBA resultsColor = averageColor / validCount;
								resultsColor.a = 1.0f;
								*pixel = resultsColor.GetReverse();
							}
						}
					}));
				}
			}
			for (auto& process : processes)
			{
				process.get();
			}
		}

		sl.Lock();
		for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
		{
			context->Unmap(resourceData->generateMipsData.texture2D.Get(), mipLevel);
		}
		sl.Unlock();

		logger::debug("{}::{} : Generate Mips done", _func_, resourceData->textureName);

		if (Config::GetSingleton().GetGenerateMipsTime())
			PerformanceLog(_func_ + "::" + resourceData->textureName
							  , true, false);
		return true;
	}
	bool ObjectNormalMapUpdater::GenerateMipsGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* srvInOut, ID3D11Texture2D* texInOut)
	{
		if (!device || !context || !srvInOut || !texInOut)
			return false;

		const auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(device, GenerateMipsShaderName.data());
		if (!shader)
			return false;

		Shader::ShaderLocker sl(context);

		HRESULT hr;
		const std::string _func_ = __func__;

		logger::info("{}::{} : Generate Mips...", _func_, resourceData->textureName);

		//create buffers
		struct ConstBufferData
		{
			UINT width;
			UINT height;
			UINT widthStart;
			UINT heightStart;

			UINT mipLevel;
			UINT padding1;
			UINT srcWidth;
			UINT srcHeight;
		};
		static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");

		struct ShaderBackup {
		public:
			void Backup(ID3D11DeviceContext* context) {
				context->CSGetShader(&shader, nullptr, 0);
				context->CSGetConstantBuffers(0, 1, &constBuffer);
				context->CSGetShaderResources(0, 1, &src0);
				context->CSGetUnorderedAccessViews(0, 1, &dst);
				context->CSGetUnorderedAccessViews(1, 1, &src);
				Unbind(context);
			}
			void Revert(ID3D11DeviceContext* context) {
				Unbind(context);
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, src0.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, dst.GetAddressOf(), nullptr);
				context->CSSetUnorderedAccessViews(1, 1, src.GetAddressOf(), nullptr);
			}
		private:
			void Unbind(ID3D11DeviceContext* context) {
				ID3D11Buffer* emptyBuffer[1] = { nullptr };
				ID3D11ShaderResourceView* emptySRV[1] = { nullptr };
				ID3D11UnorderedAccessView* emptyUAV[2] = { nullptr };
				context->CSSetShader(nullptr, nullptr, 0);
				context->CSSetConstantBuffers(0, 1, emptyBuffer);
				context->CSSetShaderResources(0, 1, emptySRV);
				context->CSSetUnorderedAccessViews(0, 2, emptyUAV, nullptr);
			}

			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> src0;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dst;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> src;
		};

		D3D11_TEXTURE2D_DESC desc;
		texInOut->GetDesc(&desc);
		const UINT width = desc.Width;
		const UINT height = desc.Height;

		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DEFAULT;
		cbDesc.ByteWidth = sizeof(ConstBufferData);
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = 0;
		cbDesc.MiscFlags = 0;
		cbDesc.StructureByteStride = 0;

		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> srcUAV = nullptr;
		for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++)
		{
			if (mipLevel > 0 && Config::GetSingleton().GetGPUForceSync())
				WaitForGPU(device, context).Wait();

			const UINT mipWidth = std::max(width >> mipLevel, 1u);
			const UINT mipHeight = std::max(height >> mipLevel, 1u);

			if (mipLevel == 0)
			{
				hr = device->CreateTexture2D(&desc, nullptr, &resourceData->generateMipsData.texture2D);
				if (FAILED(hr)) {
					logger::error("{} : Failed to create texture 2d ({})", __func__, hr);
					return false;
				}

				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				srvInOut->GetDesc(&srvDesc);
				hr = device->CreateShaderResourceView(resourceData->generateMipsData.texture2D.Get(), &srvDesc, &resourceData->generateMipsData.srv);
				if (FAILED(hr)) {
					logger::error("{} : Failed to create shader resource view ({})", __func__, hr);
					return false;
				}

				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTex = resourceData->generateMipsData.texture2D;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSRV = srvInOut;

				ConstBufferData cbData = {};
				cbData.width = mipWidth;
				cbData.height = mipHeight;
				cbData.widthStart = 0;
				cbData.heightStart = 0;
				cbData.mipLevel = 0;
				cbData.srcWidth = mipWidth;
				cbData.srcHeight = mipHeight;

				for (std::uint8_t i = 0; i < 2; i++)
				{
					if (i > 0)
					{
						if (Config::GetSingleton().GetGPUForceSync())
							WaitForGPU(device, context).Wait();
						srcTex = texInOut;
						srcSRV = resourceData->generateMipsData.srv;
					}

					Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstUAV = nullptr;
					D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
					uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
					uavDesc.Texture2D.MipSlice = 0;
					hr = device->CreateUnorderedAccessView(srcTex.Get(), &uavDesc, &dstUAV);
					if (FAILED(hr)) {
						logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
						return false;
					}
					resourceData->generateMipsData.uavs.push_back(dstUAV);

					if (sl.IsSecondGPU() || isNoSplitGPU || (256 >= mipWidth && 256 >= mipHeight))
					{
						D3D11_SUBRESOURCE_DATA cbInitData = {};
						cbInitData.pSysMem = &cbData;
						Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
						hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
						if (FAILED(hr)) {
							logger::error("{} : Failed to create const buffer ({})", __func__, hr);
							return false;
						}
						resourceData->generateMipsData.constBuffers.push_back(constBuffer);

						const DirectX::XMUINT2 dispatch = { (mipWidth + 8 - 1) / 8, (mipHeight + 8 - 1) / 8 };

						if (Config::GetSingleton().GetGenerateMipsTime())
							GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight)
											  , false, false);
						ShaderBackup sb;
						sl.Lock();
						sb.Backup(context);
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						context->CSSetShaderResources(0, 1, srcSRV.GetAddressOf());
						context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
						context->Dispatch(dispatch.x, dispatch.y, 1);
						sb.Revert(context);
						sl.Unlock();
						if (Config::GetSingleton().GetGenerateMipsTime())
							GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight)
											  , true, false);
					}
					else
					{
						const std::uint32_t subResolution = 2048 / (1u << divideTaskQ);
						const DirectX::XMUINT2 dispatch = { (std::min(mipWidth, subResolution) + 8 - 1) / 8, (std::min(mipHeight, subResolution) + 8 - 1) / 8 };
						const std::uint32_t subXSize = std::max(1u, mipWidth / subResolution);
						const std::uint32_t subYSize = std::max(1u, mipHeight / subResolution);

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
								resourceData->generateMipsData.constBuffers.push_back(constBuffer);

								gpuTasks.push_back(gpuTask->submitAsync([&, constBuffer, subX, subY, mipWidth, mipHeight, dispatch, srcSRV, dstUAV]() {
									if (Config::GetSingleton().GetGenerateMipsTime())
										GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipLevel)
														  + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight) + "::" + std::to_string(subX) + "|" + std::to_string(subY)
														  , false, false);
									ShaderBackup sb;
									sl.Lock();
									sb.Backup(context);
									context->CSSetShader(shader.Get(), nullptr, 0);
									context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
									context->CSSetShaderResources(0, 1, srcSRV.GetAddressOf());
									context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
									context->Dispatch(dispatch.x, dispatch.y, 1);
									sb.Revert(context);
									sl.Unlock();
									if (Config::GetSingleton().GetGenerateMipsTime())
										GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipLevel)
														  + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight) + "::" + std::to_string(subX) + "|" + std::to_string(subY)
														  , true, false);
								}));
							}
						}
						for (auto& task : gpuTasks)
						{
							task.get();
						}
					}
					if (i > 0)
						srcUAV = dstUAV;
				}
			}
			else
			{
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstUAV = nullptr;
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = mipLevel;
				hr = device->CreateUnorderedAccessView(texInOut, &uavDesc, &dstUAV);
				if (FAILED(hr)) {
					logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
					return false;
				}
				resourceData->generateMipsData.uavs.push_back(dstUAV);

				ConstBufferData cbData = {};
				cbData.width = mipWidth;
				cbData.height = mipHeight;
				cbData.widthStart = 0;
				cbData.heightStart = 0;
				cbData.mipLevel = mipLevel;
				cbData.srcWidth = mipLevel > 0 ? std::max(width >> (mipLevel - 1u), 1u) : mipWidth;
				cbData.srcHeight = mipLevel > 0 ? std::max(height >> (mipLevel - 1u), 1u) : mipHeight;

				if (sl.IsSecondGPU() || isNoSplitGPU || (256 >= mipWidth && 256 >= mipHeight))
				{
					const DirectX::XMUINT2 dispatch = { (mipWidth + 8 - 1) / 8, (mipHeight + 8 - 1) / 8 };

					D3D11_SUBRESOURCE_DATA cbInitData = {};
					cbInitData.pSysMem = &cbData;
					Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
					hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
					if (FAILED(hr)) {
						logger::error("{} : Failed to create const buffer ({})", __func__, hr);
						return false;
					}
					resourceData->generateMipsData.constBuffers.push_back(constBuffer);

					if (Config::GetSingleton().GetGenerateMipsTime())
						GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight)
										  , false, false);
					ShaderBackup sb;
					sl.Lock();
					sb.Backup(context);
					context->CSSetShader(shader.Get(), nullptr, 0);
					context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
					context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
					context->CSSetUnorderedAccessViews(1, 1, srcUAV.GetAddressOf(), nullptr);
					context->Dispatch(dispatch.x, dispatch.y, 1);
					sb.Revert(context);
					sl.Unlock();
					if (Config::GetSingleton().GetGenerateMipsTime())
						GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight)
										  , true, false);
				}
				else
				{
					const std::uint32_t subResolution = 2048 / (1u << divideTaskQ);
					const DirectX::XMUINT2 dispatch = { (std::min(mipWidth, subResolution) + 8 - 1) / 8, (std::min(mipHeight, subResolution) + 8 - 1) / 8 };
					const std::uint32_t subXSize = std::max(1u, mipWidth / subResolution);
					const std::uint32_t subYSize = std::max(1u, mipHeight / subResolution);

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
							resourceData->generateMipsData.constBuffers.push_back(constBuffer);

							gpuTasks.push_back(gpuTask->submitAsync([&, constBuffer, subX, subY, mipWidth, mipHeight, dispatch, srcUAV, dstUAV]() {
								if (Config::GetSingleton().GetGenerateMipsTime())
									GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipLevel)
													  + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight) + "::" + std::to_string(subX) + "|" + std::to_string(subY)
													  , false, false);
								ShaderBackup sb;
								sl.Lock();
								sb.Backup(context);
								context->CSSetShader(shader.Get(), nullptr, 0);
								context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
								context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
								context->CSSetUnorderedAccessViews(1, 1, srcUAV.GetAddressOf(), nullptr);
								context->Dispatch(dispatch.x, dispatch.y, 1);
								sb.Revert(context);
								sl.Unlock();
								if (Config::GetSingleton().GetGenerateMipsTime())
									GPUPerformanceLog(device, context, _func_ + "::" + resourceData->textureName + "::" + std::to_string(mipLevel)
													  + "::" + std::to_string(mipWidth) + "|" + std::to_string(mipHeight) + "::" + std::to_string(subX) + "|" + std::to_string(subY)
													  , true, false);
							}));
						}
					}
					for (auto& task : gpuTasks)
					{
						task.get();
					}
				}
				srcUAV = dstUAV;
			}
		}
		logger::debug("{}::{} : Generate Mips done", _func_, resourceData->textureName);
		return true;
	}

	bool ObjectNormalMapUpdater::CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		if (Config::GetSingleton().GetTextureCompress() == 0 || !device || !context || !texInOut)
			return false;

		const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
		bool isGPUCompress = false;
		if (Config::GetSingleton().GetTextureCompress() == -1)
		{
			isGPUCompress = isSecondGPU;
		}
		else
		{
			isGPUCompress = Config::GetSingleton().GetTextureCompress() == 2;
		}

		logger::info("{}::{} : Compress texture with {}...", __func__, resourceData->textureName, isGPUCompress ? "GPU" : "CPU");

		bool isCompressed = false;
		if (isGPUCompress)
		{
			D3D11_TEXTURE2D_DESC desc;
			texInOut->GetDesc(&desc);
			if (desc.Usage == D3D11_USAGE_DEFAULT)
				WaitForGPU(device, context).Wait();

			if (Config::GetSingleton().GetCompressTime())
				PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

			isCompressed = Shader::TextureLoadManager::GetSingleton().CompressTexture(device, context, DXGI_FORMAT_BC7_UNORM, isSecondGPU, texInOut);

			if (Config::GetSingleton().GetCompressTime())
				PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);
		}
		else
		{
			isCompressed = CompressTextureBC7(device, context, resourceData, texInOut);
		}

		if (!isCompressed || !texInOut)
			return false;

		logger::debug("{}::{} : Compress texture done", __func__, resourceData->textureName);
		return isCompressed;
	}
	bool ObjectNormalMapUpdater::CompressTextureBC7(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		if (!device || !context || !texInOut)
			return false;

		Shader::ShaderLocker sl(context);

		if (Config::GetSingleton().GetCompressTime())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

		HRESULT hr;

		D3D11_TEXTURE2D_DESC srcDesc;
		texInOut->GetDesc(&srcDesc); 
		D3D11_TEXTURE2D_DESC stagingDesc = srcDesc;

		if (srcDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
			return false;

		if (srcDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)
		{
			resourceData->textureCompressData.srcStagingTexture = texInOut;
		}
		else
		{
			stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			stagingDesc.Usage = D3D11_USAGE_STAGING;
			stagingDesc.ArraySize = 1;
			stagingDesc.BindFlags = 0;
			stagingDesc.MiscFlags = 0;
			stagingDesc.SampleDesc.Count = 1;
			hr = device->CreateTexture2D(&stagingDesc, nullptr, &resourceData->textureCompressData.srcStagingTexture);
			if (FAILED(hr))
			{
				logger::error("Failed to create staging texture");
				return false;
			}
			for (UINT mipLevel = 0; mipLevel < srcDesc.MipLevels; mipLevel++) {
				CopySubresourceRegion(device, context, resourceData->textureCompressData.srcStagingTexture.Get(), texInOut.Get(), mipLevel, mipLevel);
			}
		}

		std::vector<D3D11_MAPPED_SUBRESOURCE> mappedResource(srcDesc.MipLevels);
		sl.Lock();
		for (UINT mipLevel = 0; mipLevel < srcDesc.MipLevels; mipLevel++)
		{
			context->Map(resourceData->textureCompressData.srcStagingTexture.Get(), mipLevel, D3D11_MAP_READ, 0, &mappedResource[mipLevel]);
		}
		sl.Unlock();

		std::vector<D3D11_SUBRESOURCE_DATA> initData(srcDesc.MipLevels);
		std::vector<std::vector<std::uint8_t>> bc7Buffers(srcDesc.MipLevels);

		bc7enc_compress_block_init();

		bc7enc_compress_block_params params;
		bc7enc_compress_block_params_init(&params);
		bc7enc_compress_block_params_init_linear_weights(&params);
		params.m_mode_mask = 1 << 6;
		params.m_max_partitions = 1;
		params.m_uber_level = 0;

		for (UINT mipLevel = 0; mipLevel < srcDesc.MipLevels; mipLevel++)
		{
			UINT width = std::max(1u, srcDesc.Width >> mipLevel);
			UINT height = std::max(1u, srcDesc.Height >> mipLevel);
			UINT bc7Width = (width + 4 - 1) / 4;
			UINT bc7Height = (height + 4 - 1) / 4;
			bc7Buffers[mipLevel].resize((std::size_t)bc7Width * bc7Height * 16);

			std::uint8_t* srcData = reinterpret_cast<std::uint8_t*>(mappedResource[mipLevel].pData);
			std::vector<std::future<void>> processes;
			const std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(bc7Width), processingThreads->GetThreads() * 4));
			const std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(bc7Height), processingThreads->GetThreads() * 4));
			const std::size_t unitX = (std::size_t(bc7Width) + subX - 1) / subX;
			const std::size_t unitY = (std::size_t(bc7Height) + subY - 1) / subY;
			for (UINT sby = 0; sby < subY; sby++) {
				for (UINT sbx = 0; sbx < subX; sbx++)
				{
					const std::size_t beginX = sbx * unitX;
					const std::size_t endX = std::min(beginX + unitX, std::size_t(bc7Width));
					const std::size_t beginY = sby * unitY;
					const std::size_t endY = std::min(beginY + unitY, std::size_t(bc7Height));
					processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
						for (UINT by = beginY; by < endY; by++) {
							for (UINT bx = beginX; bx < endX; bx++) {
								const std::size_t currentBlockIndex = (std::size_t)by * bc7Width + bx;
								std::vector<std::uint8_t> block(64);
								for (UINT y = 0; y < 4; y++) {
									for (UINT x = 0; x < 4; x++) {
										UINT px = bx * 4 + x;
										UINT py = by * 4 + y;
										UINT clampedPx = std::min(px, width - 1);
										UINT clampedPy = std::min(py, height - 1);

										std::uint32_t* dstPixel = reinterpret_cast<std::uint32_t*>(srcData + clampedPy * mappedResource[mipLevel].RowPitch + clampedPx * 4);
										RGBA rgba;
										rgba.SetReverse(*dstPixel);
										std::uint8_t* dst = block.data() + (y * 4 + x) * 4;
										dst[0] = rgba.R();
										dst[1] = rgba.G();
										dst[2] = rgba.B();
										dst[3] = rgba.A();
									}
								}
								std::uint8_t* outputPtr = bc7Buffers[mipLevel].data() + currentBlockIndex * 16;
								bc7enc_compress_block(outputPtr, block.data(), &params);
							}
						}
					}));
				}
			}
			for (auto& process : processes) {
				process.get();
			}
			initData[mipLevel].pSysMem = bc7Buffers[mipLevel].data();
			initData[mipLevel].SysMemPitch = bc7Width * 16;
			initData[mipLevel].SysMemSlicePitch = 0;
		};

		sl.Lock();
		for (UINT mipLevel = 0; mipLevel < srcDesc.MipLevels; mipLevel++)
		{
			context->Unmap(resourceData->textureCompressData.srcStagingTexture.Get(), mipLevel);
		}
		sl.Unlock();


		D3D11_TEXTURE2D_DESC dstDesc = srcDesc;
		if (sl.IsSecondGPU())
		{
			dstDesc.Format = DXGI_FORMAT_BC7_UNORM;
			dstDesc.Usage = D3D11_USAGE_STAGING;
			dstDesc.BindFlags = 0;
			dstDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			dstDesc.MiscFlags = 0;

			hr = device->CreateTexture2D(&dstDesc, initData.data(), texInOut.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to create dst texture 2d");
				return false;
			}
		}
		else
		{
			stagingDesc.Format = DXGI_FORMAT_BC7_UNORM;
			stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			hr = device->CreateTexture2D(&stagingDesc, nullptr, &resourceData->textureCompressData.dstStagingTexture);
			if (FAILED(hr))
			{
				logger::error("Failed to create dst texture 2d");
				return false;
			}

			for (UINT mipLevel = 0; mipLevel < stagingDesc.MipLevels; mipLevel++) {
				const UINT height = std::max(stagingDesc.Height >> mipLevel, 1u);
				std::size_t blockHeight = std::size_t(height + 3) / 4;

				D3D11_MAPPED_SUBRESOURCE mapped;
				sl.Lock();
				context->Map(resourceData->textureCompressData.dstStagingTexture.Get(), mipLevel, D3D11_MAP_WRITE, 0, &mapped);
				sl.Unlock();
				if (FAILED(hr))
				{
					logger::error("Failed to map copy texture : {}", hr);
					return false;
				}
				const std::size_t rowPitch = mapped.RowPitch;

				for (std::size_t y = 0; y < blockHeight; y++) {
					memcpy(reinterpret_cast<std::uint8_t*>(mapped.pData) + y * rowPitch, bc7Buffers[mipLevel].data() + y * rowPitch, rowPitch);
				}

				sl.Lock();
				context->Unmap(resourceData->textureCompressData.dstStagingTexture.Get(), mipLevel);
				sl.Unlock();
			}

			dstDesc.Format = DXGI_FORMAT_BC7_UNORM;
			dstDesc.Usage = D3D11_USAGE_DEFAULT;
			dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			dstDesc.CPUAccessFlags = 0;
			dstDesc.MiscFlags = 0; 
			hr = device->CreateTexture2D(&dstDesc, nullptr, texInOut.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to create dst texture 2d");
				return false;
			}
			for (UINT mipLevel = 0; mipLevel < dstDesc.MipLevels; mipLevel++) {
				CopySubresourceRegion(device, context, texInOut.Get(), resourceData->textureCompressData.dstStagingTexture.Get(), mipLevel, mipLevel);
			}
		}

		if (Config::GetSingleton().GetCompressTime())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);
		return true;
	}

	bool ObjectNormalMapUpdater::CopyResourceSecondToMain(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut)
	{
		if (!Shader::ShaderManager::GetSingleton().IsValidSecondGPU() || !texInOut)
			return false;

		logger::info("{}::{} : Copy resource from sub to main GPU...", __func__, resourceData->textureName);

		HRESULT hr;

		D3D11_TEXTURE2D_DESC desc;
		texInOut->GetDesc(&desc);

		if (desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ)
		{
			resourceData->copySecondToMainData.copyTexture2D = texInOut;
		}
		else
		{
			WaitForGPU(Shader::ShaderManager::GetSingleton().GetSecondDevice(), Shader::ShaderManager::GetSingleton().GetSecondContext()).Wait();

			D3D11_TEXTURE2D_DESC copyDesc = desc;
			copyDesc.Usage = D3D11_USAGE_STAGING;
			copyDesc.BindFlags = 0;
			copyDesc.MiscFlags = 0;
			copyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			copyDesc.ArraySize = 1;
			copyDesc.SampleDesc.Count = 1;

			hr = Shader::ShaderManager::GetSingleton().GetSecondDevice()->CreateTexture2D(&copyDesc, nullptr, &resourceData->copySecondToMainData.copyTexture2D);
			if (FAILED(hr)) {
				logger::error("Failed to create copy texture : {}", hr);
				return false;
			}

			Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
			Shader::ShaderManager::GetSingleton().GetSecondContext()->CopyResource(resourceData->copySecondToMainData.copyTexture2D.Get(), texInOut.Get());
			Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
		}

		std::vector<D3D11_SUBRESOURCE_DATA> initDatas(desc.MipLevels);
		std::vector<std::vector<std::uint8_t>> buffers(desc.MipLevels);

		for (UINT mipLevel = 0; mipLevel < desc.MipLevels; mipLevel++) {
			const UINT height = std::max(desc.Height >> mipLevel, 1u);
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

			if (desc.Format == DXGI_FORMAT_BC7_UNORM || desc.Format == DXGI_FORMAT_BC3_UNORM || desc.Format == DXGI_FORMAT_BC1_UNORM)
			{
				blockHeight = std::size_t(height + 3) / 4;
			}
			else if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
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
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		hr = Shader::ShaderManager::GetSingleton().GetDevice()->CreateTexture2D(&desc, initDatas.data(), &texture2D);
		if (FAILED(hr)) {
			logger::error("Failed to create dst texture : {}", hr);
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;

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

		Shader::ShaderLocker sl(context);

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

			sl.Lock();
			context->Begin(gpuTimers[funcStr].disjointQuery.Get());
			context->End(gpuTimers[funcStr].startQuery.Get());
			sl.Unlock();
		}
		else
		{
			double tick = PerformanceCheckTick ? (double)(RE::GetSecondsSinceLastFrame() * 1000) : (double)(TimeTick60 * 1000);
			sl.Lock();
			context->Flush();
			context->End(gpuTimers[funcStr].endQuery.Get());
			context->End(gpuTimers[funcStr].disjointQuery.Get());
			sl.Unlock();

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData = {};
			sl.Lock();
			while (context->GetData(gpuTimers[funcStr].disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) != S_OK);
			sl.Unlock();

			if (disjointData.Disjoint)
				return;

			UINT64 startTime = 0, endTime = 0;
			sl.Lock();
			while (context->GetData(gpuTimers[funcStr].startQuery.Get(), &startTime, sizeof(startTime), 0) != S_OK);
			while (context->GetData(gpuTimers[funcStr].endQuery.Get(), &endTime, sizeof(endTime), 0) != S_OK);
			sl.Unlock();

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

	WaitForGPU::WaitForGPU(ID3D11Device* a_device, ID3D11DeviceContext* a_context)
		: device(a_device), context(a_context), sl(a_context)
	{
		if (!device || !context)
			return;

		D3D11_QUERY_DESC queryDesc = {};
		queryDesc.Query = D3D11_QUERY_EVENT;
		queryDesc.MiscFlags = 0;

		HRESULT hr = device->CreateQuery(&queryDesc, &query);
		if (FAILED(hr)) {
			return;
		}
		sl.Lock();
		context->Flush();
		context->End(query.Get());
		sl.Unlock();
		logger::trace("Wait for Renderer...");
	}

	void WaitForGPU::Wait()
	{
		if (!device || !context || !query)
			return;
		HRESULT hr;
		while (true) {
			sl.Lock();
			hr = context->GetData(query.Get(), nullptr, 0, 0);
			sl.Unlock();
			if (FAILED(hr))
				break;
			if (hr == S_OK)
				break;
			std::this_thread::sleep_for(waitSleepTime);
		}
		logger::trace("Wait for Renderer done");
		return;
	}
}
