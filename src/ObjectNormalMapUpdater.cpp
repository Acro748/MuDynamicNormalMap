#include "ObjectNormalMapUpdater.h"
#include "bc7enc_rdo/bc7enc.h"

namespace Mus {
	void ObjectNormalMapUpdater::onEvent(const FrameEvent& e)
	{
		static std::clock_t lastTickTime = currentTime;
		if (currentTime - lastTickTime < 100) //0.1sec
			return;
		lastTickTime = currentTime;
		memoryManageThreads->submitAsync([&]() {
			ResourceDataMapLock.lock();
			auto ResourceDataMap_ = ResourceDataMap;
			ResourceDataMap.clear();
			ResourceDataMapLock.unlock();
			for (auto& map : ResourceDataMap_)
			{
				if (map->time < 0)
				{
					if (map->IsQueryDone(Shader::ShaderManager::GetSingleton().GetContext()))
						map->time = currentTime;
				}
				else if (currentTime - map->time > Config::GetSingleton().GetWaitForRendererTickMS()) //1sec
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
			for (auto& map : GeometryResourceDataMap)
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
			if (GeometryResourceDataMap.size() > 0)
				logger::debug("Current remain geometry resource {}", GeometryResourceDataMap.size());
		});
	}

	void ObjectNormalMapUpdater::Init()
	{
		if (Config::GetSingleton().GetGPUEnable())
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
			auto device = Shader::ShaderManager::GetSingleton().GetDevice();
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
			Shader::ShaderManager::GetSingleton().GetComputeShader(UpdateNormalMapShaderName.data());
		}
		if (Config::GetSingleton().GetTextureMarginGPU())
		{
			Shader::ShaderManager::GetSingleton().GetComputeShader(BleedTextureShaderName.data());
		}
		if (Config::GetSingleton().GetMergeTextureGPU())
		{
			Shader::ShaderManager::GetSingleton().GetComputeShader(MergeTextureShaderName.data());
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
			GeometryResourceDataPtr newGeometryResourceData = std::make_shared<GeometryResourceData>();
			if (!CreateStructuredBuffer(a_data->vertices.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->vertices.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->vertexBuffer, newGeometryResourceData->vertexSRV))
				return false;
			if (!CreateStructuredBuffer(a_data->uvs.data(), UINT(sizeof(DirectX::XMFLOAT2) * a_data->uvs.size()), sizeof(DirectX::XMFLOAT2), newGeometryResourceData->uvBuffer, newGeometryResourceData->uvSRV))
				return false;
			if (!CreateStructuredBuffer(a_data->normals.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->normals.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->normalBuffer, newGeometryResourceData->normalSRV))
				return false;
			if (!CreateStructuredBuffer(a_data->tangents.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->tangents.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->tangentBuffer, newGeometryResourceData->tangentSRV))
				return false;
			if (!CreateStructuredBuffer(a_data->bitangents.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data->bitangents.size()), sizeof(DirectX::XMFLOAT3), newGeometryResourceData->bitangentBuffer, newGeometryResourceData->bitangentSRV))
				return false;
			if (!CreateStructuredBuffer(a_data->indices.data(), UINT(sizeof(std::uint32_t) * a_data->indices.size()), sizeof(std::uint32_t), newGeometryResourceData->indicesBuffer, newGeometryResourceData->indicesSRV))
				return false;
			ResourceDataMapLock.lock_shared();
			auto found = GeometryResourceDataMap.find(a_actorID);
			if (found == GeometryResourceDataMap.end())
			{
				GeometryResourceDataMap.insert(std::make_pair(a_actorID, newGeometryResourceData));
			}
			else
			{
				found->second = newGeometryResourceData;
			}
			ResourceDataMapLock.unlock_shared();
		}
		return true;
	}

	ObjectNormalMapUpdater::UpdateResult ObjectNormalMapUpdater::UpdateObjectNormalMap(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet)
	{
		std::string_view _func_ = __func__;

		UpdateResult result;

		HRESULT hr;
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
		{
			logger::error("{}::{:x} : Invalid renderer", _func_, a_actorID);
			return result;
		}

		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_updateSet.size());

		for (auto& update : a_updateSet)
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
				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
				D3D11_TEXTURE2D_DESC srcDesc;
				D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
				if (!IsDetailNormalMap(update.second.srcTexturePath))
				{
					logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.srcTexturePath);

					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.srcTexturePath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM, srcTexture2D))
					{
						dstDesc = srcDesc;
						dstDesc.Width = Config::GetSingleton().GetTextureWidth();
						dstDesc.Height = Config::GetSingleton().GetTextureHeight();

						srcStagingDesc = srcDesc;
						srcStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
						srcStagingDesc.Usage = D3D11_USAGE_STAGING;
						srcStagingDesc.BindFlags = 0;
						srcStagingDesc.MiscFlags = 0;
						srcStagingDesc.MipLevels = 1;
						srcStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

						dstShaderResourceViewDesc = srcShaderResourceViewDesc;
						hr = device->CreateTexture2D(&srcStagingDesc, nullptr, &newResourceData->srcTexture2D);
						if (FAILED(hr))
						{
							logger::error("{}::{:x} : Failed to create src staging texture ({}|{})", _func_, a_actorID, hr, update.second.srcTexturePath);
							newResourceData->srcTexture2D = nullptr;
						}
						else
						{
							CopySubresourceRegion(newResourceData->srcTexture2D, srcTexture2D, 0, 0);
						}
					}
				}
			}
			if (!update.second.detailTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> detailTexture2D = nullptr;
				D3D11_TEXTURE2D_DESC detailDesc;
				D3D11_SHADER_RESOURCE_VIEW_DESC detailShaderResourceViewDesc;
				logger::info("{}::{:x}::{} : {} detail texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.detailTexturePath);

				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.detailTexturePath, detailDesc, detailShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM, detailTexture2D))
				{
					dstDesc.Width = std::max(dstDesc.Width, Config::GetSingleton().GetTextureWidth());
					dstDesc.Height = std::max(dstDesc.Height, Config::GetSingleton().GetTextureHeight());

					detailStagingDesc = detailDesc;
					detailStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					detailStagingDesc.Usage = D3D11_USAGE_STAGING;
					detailStagingDesc.BindFlags = 0;
					detailStagingDesc.MiscFlags = 0;
					detailStagingDesc.MipLevels = 1;
					detailStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

					dstShaderResourceViewDesc = detailShaderResourceViewDesc;
					hr = device->CreateTexture2D(&detailStagingDesc, nullptr, &newResourceData->detailTexture2D);
					if (FAILED(hr))
					{
						logger::error("{}::{:x} : Failed to create detail staging texture ({}|{})", _func_, a_actorID, hr, update.second.detailTexturePath);
						newResourceData->detailTexture2D = nullptr;
					}
					else
					{
						CopySubresourceRegion(newResourceData->detailTexture2D, detailTexture2D, 0, 0);
					}
				}

			}
			if (!newResourceData->srcTexture2D && !newResourceData->detailTexture2D)
			{
				logger::error("{}::{:x}::{} : There is no Normalmap!", _func_, a_actorID, update.second.geometryName);
				continue;
			}

			if (!update.second.overlayTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> overlayTexture2D = nullptr;
				logger::info("{}::{:x}::{} : {} overlay texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.overlayTexturePath);
				D3D11_TEXTURE2D_DESC overlayDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.overlayTexturePath, overlayDesc, DXGI_FORMAT_R8G8B8A8_UNORM, overlayTexture2D))
				{
					overlayStagingDesc = overlayDesc;
					overlayStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					overlayStagingDesc.Usage = D3D11_USAGE_STAGING;
					overlayStagingDesc.BindFlags = 0;
					overlayStagingDesc.MiscFlags = 0;
					overlayStagingDesc.MipLevels = 1;
					overlayStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
					hr = device->CreateTexture2D(&overlayStagingDesc, nullptr, &newResourceData->overlayTexture2D);
					if (FAILED(hr))
					{
						logger::error("{}::{:x}::{} : Failed to create overlay staging texture ({})", _func_, a_actorID, update.second.geometryName, hr);
						newResourceData->overlayTexture2D = nullptr;
					}
					else
					{
						CopySubresourceRegion(newResourceData->overlayTexture2D, overlayTexture2D, 0, 0);
					}
				}
			}

			if (!update.second.maskTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> maskTexture2D = nullptr;
				logger::info("{}::{:x}::{} : {} mask texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.maskTexturePath);
				D3D11_TEXTURE2D_DESC maskDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.maskTexturePath, maskDesc, DXGI_FORMAT_R8G8B8A8_UNORM, maskTexture2D))
				{
					maskStagingDesc = maskDesc;
					maskStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					maskStagingDesc.Usage = D3D11_USAGE_STAGING;
					maskStagingDesc.BindFlags = 0;
					maskStagingDesc.MiscFlags = 0;
					maskStagingDesc.MipLevels = 1;
					maskStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
					hr = device->CreateTexture2D(&maskStagingDesc, nullptr, &newResourceData->maskTexture2D);
					if (FAILED(hr))
					{
						logger::error("{}::{:x}::{} : Failed to create mask staging texture ({})", _func_, a_actorID, update.second.geometryName, hr);
						newResourceData->maskTexture2D = nullptr;
					}
					else
					{
						CopySubresourceRegion(newResourceData->maskTexture2D, maskTexture2D, 0, 0);
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

			WaitForGPU();

			D3D11_MAPPED_SUBRESOURCE srcMappedResource;
			uint8_t* srcData = nullptr;
			if (newResourceData->srcTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(newResourceData->srcTexture2D.Get(), 0, D3D11_MAP_READ, 0, &srcMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				srcData = reinterpret_cast<uint8_t*>(srcMappedResource.pData);
			}
			D3D11_MAPPED_SUBRESOURCE detailMappedResource;
			uint8_t* detailData = nullptr;
			if (newResourceData->detailTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(newResourceData->detailTexture2D.Get(), 0, D3D11_MAP_READ, 0, &detailMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				detailData = reinterpret_cast<uint8_t*>(detailMappedResource.pData);
			}
			D3D11_MAPPED_SUBRESOURCE overlayMappedResource;
			uint8_t* overlayData = nullptr;
			if (newResourceData->overlayTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(newResourceData->overlayTexture2D.Get(), 0, D3D11_MAP_READ, 0, &overlayMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				overlayData = reinterpret_cast<uint8_t*>(overlayMappedResource.pData);
			}
			D3D11_MAPPED_SUBRESOURCE maskMappedResource;
			uint8_t* maskData = nullptr;
			if (newResourceData->maskTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(newResourceData->maskTexture2D.Get(), 0, D3D11_MAP_READ, 0, &maskMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				maskData = reinterpret_cast<uint8_t*>(maskMappedResource.pData);
			}

			const bool hasSrcData = (srcData != nullptr);
			const bool hasDetailData = (detailData != nullptr);
			const bool hasOverlayData = (overlayData != nullptr);
			const bool hasMaskData = (maskData != nullptr);

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			hr = context->Map(newResourceData->dstWriteTexture2D.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mappedResource);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
			{
				logger::error("{}::{:x}::{} Failed to read data from the staging texture ({})", _func_, a_actorID, update.second.geometryName, hr);
				continue;
			}

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
											const DirectX::XMVECTOR b01 = SlerpVector(b0v, b1v, bary.y / denomal);

											const DirectX::XMVECTOR t = SlerpVector(t01, t2v, bary.z);
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

								std::uint32_t* dstPixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
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

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->Unmap(newResourceData->dstWriteTexture2D.Get(), 0);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			if (hasSrcData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(newResourceData->srcTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
			if (hasDetailData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(newResourceData->detailTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
			if (hasOverlayData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(newResourceData->overlayTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
			if (hasMaskData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(newResourceData->maskTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}

			dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstDesc.Usage = D3D11_USAGE_DEFAULT;
			dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			dstDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			dstDesc.MipLevels = 0;
			dstDesc.ArraySize = 1;
			dstDesc.CPUAccessFlags = 0;
			dstDesc.SampleDesc.Count = 1;
			hr = device->CreateTexture2D(&dstDesc, nullptr, &newResourceData->dstTexture2D);
			if (FAILED(hr))
			{
				logger::error("{}::{:x} : Failed to create dst texture ({})", _func_, a_actorID, hr);
				continue;
			}

			CopySubresourceRegion(newResourceData->dstTexture2D, newResourceData->dstWriteTexture2D, 0, 0);

			dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
			dstShaderResourceViewDesc.Texture2D.MipLevels = -1;
			hr = device->CreateShaderResourceView(newResourceData->dstTexture2D.Get(), &dstShaderResourceViewDesc, &newResourceData->dstShaderResourceView);
			if (FAILED(hr)) {
				logger::error("{}::{:x} : Failed to create ShaderResourceView ({})", _func_, a_actorID, hr);
				continue;
			}

			if (Config::GetSingleton().GetTextureMarginGPU())
				BleedTextureGPU(newResourceData, Config::GetSingleton().GetTextureMargin(), newResourceData->dstShaderResourceView, newResourceData->dstTexture2D);
			else
				BleedTexture(newResourceData, Config::GetSingleton().GetTextureMargin(), newResourceData->dstTexture2D);

			auto resultFound = std::find_if(result.begin(), result.end(), [&](NormalMapResult& a_result) {
				return update.second.textureName == a_result.textureName;
			});
			if (resultFound != result.end()) {
				logger::info("{}::{:x} : Merge texture into {}...", _func_, a_actorID, resultFound->geoName);
				if (Config::GetSingleton().GetMergeTextureGPU())
					MergeTextureGPU(newResourceData, newResourceData->dstShaderResourceView, newResourceData->dstTexture2D, resultFound->normalmapShaderResourceView, resultFound->normalmapTexture2D);
				else
					MergeTexture(newResourceData, newResourceData->dstTexture2D, resultFound->normalmapTexture2D);
				resultFound->normalmapTexture2D = newResourceData->dstTexture2D;
				resultFound->normalmapShaderResourceView = newResourceData->dstShaderResourceView;
			}

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->GenerateMips(newResourceData->dstShaderResourceView.Get());
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			bool isCompress = CompressTexture(newResourceData, newResourceData->dstCompressTexture2D, newResourceData->dstCompressShaderResourceView, newResourceData->dstTexture2D, newResourceData->dstShaderResourceView);

			newResourceData->GetQuery(device, context);

			NormalMapResult newNormalMapResult;
			newNormalMapResult.slot = update.second.slot;
			newNormalMapResult.geometry = update.first;
			newNormalMapResult.vertexCount = objInfo.vertexCount();
			newNormalMapResult.geoName = update.second.geometryName;
			newNormalMapResult.textureName = update.second.textureName;
			newNormalMapResult.normalmapTexture2D = isCompress ? newResourceData->dstCompressTexture2D : newResourceData->dstTexture2D;
			newNormalMapResult.normalmapShaderResourceView = isCompress ? newResourceData->dstCompressShaderResourceView : newResourceData->dstShaderResourceView;
			result.push_back(newNormalMapResult);

			ResourceDataMapLock.lock_shared();
			ResourceDataMap.push_back(newResourceData);
			ResourceDataMapLock.unlock_shared();
			logger::info("{}::{:x}::{} : normalmap updated", _func_, a_actorID, update.second.geometryName);
		}
		return result;
	}

	ObjectNormalMapUpdater::UpdateResult ObjectNormalMapUpdater::UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet)
	{
		std::string_view _func_ = __func__;

		UpdateResult result;
		if (!samplerState)
		{
			logger::error("{}::{:x} : Invalid SampleState", _func_, a_actorID);
			return result;
		}

		HRESULT hr;
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
		{
			logger::error("{}::{:x} : Invalid renderer", _func_, a_actorID);
			return result;
		}
		ResourceDataMapLock.lock_shared();
		auto geoDataFound = GeometryResourceDataMap.find(a_actorID);
		if (geoDataFound == GeometryResourceDataMap.end())
		{
			logger::error("{}::{:x} : Invalid geometry data", _func_, a_actorID);
			ResourceDataMapLock.unlock_shared();
			return result;
		}
		auto& geoData = geoDataFound->second;
		ResourceDataMapLock.unlock_shared();
		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_updateSet.size());

		for (auto& update : a_updateSet)
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

			//Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D, detailTexture2D, overlayTexture2D, maskTexture2D;
			//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcShaderResourceView, detailShaderResourceView, overlayShaderResourceView, maskShaderResourceView;
			D3D11_TEXTURE2D_DESC srcDesc = {}, detailDesc = {}, overlayDesc = {}, maskDesc = {}, dstDesc = {}, dstWriteDesc = {};
			D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};

			if (!update.second.srcTexturePath.empty())
			{
				if (!IsDetailNormalMap(update.second.srcTexturePath))
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
					logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.srcTexturePath);
					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.srcTexturePath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, newResourceData->srcTexture2D))
					{
						dstDesc = srcDesc;
						dstDesc.Width = Config::GetSingleton().GetTextureWidth();
						dstDesc.Height = Config::GetSingleton().GetTextureHeight();
						dstShaderResourceViewDesc = srcShaderResourceViewDesc;

						srcShaderResourceViewDesc.Texture2D.MipLevels = 1;
						hr = device->CreateShaderResourceView(newResourceData->srcTexture2D.Get(), &srcShaderResourceViewDesc, newResourceData->srcShaderResourceView.ReleaseAndGetAddressOf());
						if (FAILED(hr))
						{
							logger::error("{}::{:x}::{} : Failed to create src shader resource view ({}|{})", _func_, a_actorID, update.second.geometryName, hr, update.second.srcTexturePath);
							newResourceData->srcShaderResourceView = nullptr;
						}
					}
				}
			}
			if (!update.second.detailTexturePath.empty())
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC detailShaderResourceViewDesc;
				logger::info("{}::{:x}::{} : {} detail texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.detailTexturePath);
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.detailTexturePath, detailDesc, detailShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, newResourceData->detailTexture2D))
				{
					dstDesc = detailDesc;
					dstDesc.Width = std::max(dstDesc.Width, Config::GetSingleton().GetTextureWidth());
					dstDesc.Height = std::max(dstDesc.Height, Config::GetSingleton().GetTextureHeight());

					dstShaderResourceViewDesc = detailShaderResourceViewDesc;

					detailShaderResourceViewDesc.Texture2D.MipLevels = 1;
					hr = device->CreateShaderResourceView(newResourceData->detailTexture2D.Get(), &detailShaderResourceViewDesc, newResourceData->detailShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr))
					{
						logger::error("{}::{:x}::{} : Failed to create detail shader resource view ({}|{})", _func_, a_actorID, update.second.geometryName, hr, update.second.detailTexturePath);
						newResourceData->detailShaderResourceView = nullptr;
					}
				}
			}
			if (!newResourceData->srcShaderResourceView && !newResourceData->detailShaderResourceView)
			{
				logger::error("{}::{:x}::{} : There is no Normalmap!", _func_, a_actorID, update.second.geometryName);
				continue;
			}

			if (!update.second.overlayTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} overlay texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.overlayTexturePath);
				D3D11_SHADER_RESOURCE_VIEW_DESC overlayShaderResourceViewDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.overlayTexturePath, overlayDesc, overlayShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, newResourceData->overlayTexture2D))
				{
					overlayShaderResourceViewDesc.Texture2D.MipLevels = 1;
					hr = device->CreateShaderResourceView(newResourceData->overlayTexture2D.Get(), &overlayShaderResourceViewDesc, newResourceData->overlayShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr))
					{
						newResourceData->overlayShaderResourceView = nullptr;
						logger::error("{}::{:x}::{} : Failed to create overlay shader resource view ({}|{})", _func_, a_actorID, update.second.geometryName, hr, update.second.overlayTexturePath);
					}
				}
			}

			if (!update.second.maskTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} mask texture loading...)", _func_, a_actorID, update.second.geometryName, update.second.maskTexturePath);
				D3D11_SHADER_RESOURCE_VIEW_DESC maskShaderResourceViewDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(update.second.maskTexturePath, maskDesc, maskShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, newResourceData->maskTexture2D))
				{
					maskShaderResourceViewDesc.Texture2D.MipLevels = 1;
					hr = device->CreateShaderResourceView(newResourceData->maskTexture2D.Get(), &maskShaderResourceViewDesc, newResourceData->maskShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr))
					{
						newResourceData->maskShaderResourceView = nullptr;
						logger::error("{}::{:x}::{} : Failed to create mask shader resource view ({}|{})", _func_, a_actorID, update.second.geometryName, hr, update.second.maskTexturePath);
					}
				}
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
				continue;
			}

			D3D11_UNORDERED_ACCESS_VIEW_DESC dstUnorderedViewDesc = {};
			dstUnorderedViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstUnorderedViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			dstUnorderedViewDesc.Texture2D.MipSlice = 0;
			hr = device->CreateUnorderedAccessView(newResourceData->dstWriteTexture2D.Get(), &dstUnorderedViewDesc, &newResourceData->dstWriteTextureUAV);
			if (FAILED(hr))
			{
				logger::error("{}::{:x}::{} : Failed to create dst unordered access view ({})", _func_, a_actorID, update.second.geometryName, hr);
				continue;
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

			auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(UpdateNormalMapShaderName.data());
			if (!shader)
			{
				logger::error("{}::{:x} : Invalid shader", _func_, a_actorID);
				continue;
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
				}
				void Revert(ID3D11DeviceContext* context) {
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
			private:
			} sb;

			std::mutex constBuffersLock;
			const std::uint32_t totalTris = objInfo.indicesCount() / 3;
			std::uint32_t subSize = std::max(std::uint32_t(1), totalTris / 10000);

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
					return result;
				}
				gpuTasks.push_back(gpuTask->submitAsync([&, constBuffer, subIndex]() {
					if (Config::GetSingleton().GetUpdateNormalMapTime2())
						GPUPerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName + "::" + std::to_string(subIndex), false, false);

					Shader::ShaderManager::GetSingleton().ShaderContextLock();
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
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					constBuffersLock.lock();
					newResourceData->constBuffers.push_back(constBuffer);
					constBuffersLock.unlock();

					if (Config::GetSingleton().GetUpdateNormalMapTime2())
						GPUPerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + update.second.geometryName + "::" + std::to_string(subIndex), true, false);
				}));
			}
			for (auto& task : gpuTasks) {
				task.get();
			}

			dstDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstDesc.Usage = D3D11_USAGE_DEFAULT;
			dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			dstDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			dstDesc.MipLevels = 0;
			dstDesc.ArraySize = 1;
			dstDesc.CPUAccessFlags = 0;
			dstDesc.SampleDesc.Count = 1;
			hr = device->CreateTexture2D(&dstDesc, nullptr, &newResourceData->dstTexture2D);
			if (FAILED(hr))
			{
				logger::error("{}::{:x} : Failed to create dst texture ({})", _func_, a_actorID, hr);
				continue;
			}

			CopySubresourceRegion(newResourceData->dstTexture2D, newResourceData->dstWriteTexture2D, 0, 0);

			dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
			dstShaderResourceViewDesc.Texture2D.MipLevels = -1;
			hr = device->CreateShaderResourceView(newResourceData->dstTexture2D.Get(), &dstShaderResourceViewDesc, &newResourceData->dstShaderResourceView);
			if (FAILED(hr)) {
				logger::error("{}::{:x} : Failed to create ShaderResourceView ({})", _func_, a_actorID, hr);
				continue;
			}

			//TexturePostProcessingGPU(newResourceData, Config::GetSingleton().GetBlueRadius(), newResourceData->maskShaderResourceView, newResourceData->dstShaderResourceView, newResourceData->dstTexture2D);
			if (Config::GetSingleton().GetTextureMarginGPU())
				BleedTextureGPU(newResourceData, Config::GetSingleton().GetTextureMargin(), newResourceData->dstShaderResourceView, newResourceData->dstTexture2D);
			else
				BleedTexture(newResourceData, Config::GetSingleton().GetTextureMargin(), newResourceData->dstTexture2D);

			const auto resultFound = std::find_if(result.begin(), result.end(), [&](NormalMapResult& a_result) {
				return update.second.textureName == a_result.textureName;
			});
			if (resultFound != result.end()) {
				logger::info("{}::{:x} : Merge texture into {}...", _func_, a_actorID, resultFound->geoName);
				if (Config::GetSingleton().GetMergeTextureGPU())
					MergeTextureGPU(newResourceData, newResourceData->dstShaderResourceView, newResourceData->dstTexture2D, resultFound->normalmapShaderResourceView, resultFound->normalmapTexture2D);
				else
					MergeTexture(newResourceData, newResourceData->dstTexture2D, resultFound->normalmapTexture2D);
				resultFound->normalmapTexture2D = newResourceData->dstTexture2D;
				resultFound->normalmapShaderResourceView = newResourceData->dstShaderResourceView;
			}

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->GenerateMips(newResourceData->dstShaderResourceView.Get());
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			const bool isCompress = CompressTexture(newResourceData, newResourceData->dstCompressTexture2D, newResourceData->dstCompressShaderResourceView, newResourceData->dstTexture2D, newResourceData->dstShaderResourceView);

			newResourceData->GetQuery(device, context);

			NormalMapResult newNormalMapResult;
			newNormalMapResult.slot = update.second.slot;
			newNormalMapResult.geometry = update.first;
			newNormalMapResult.vertexCount = objInfo.vertexCount();
			newNormalMapResult.geoName = update.second.geometryName;
			newNormalMapResult.texturePath = update.second.srcTexturePath.empty() ? update.second.detailTexturePath : update.second.srcTexturePath;
			newNormalMapResult.textureName = update.second.textureName;
			newNormalMapResult.normalmapTexture2D = isCompress ? newResourceData->dstCompressTexture2D : newResourceData->dstTexture2D;
			newNormalMapResult.normalmapShaderResourceView = isCompress ? newResourceData->dstCompressShaderResourceView : newResourceData->dstShaderResourceView;
			result.push_back(newNormalMapResult);

			ResourceDataMapLock.lock_shared();
			ResourceDataMap.push_back(newResourceData);
			ResourceDataMapLock.unlock_shared();
			logger::info("{}::{:x}::{} : normalmap updated", _func_, a_actorID, update.second.geometryName);
		}
		geoData->GetQuery(device, context);
		return result;
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

		bufferOut.Reset();
		hr = buffer.As(&bufferOut);
		if (FAILED(hr))
		{
			logger::error("Failed to move buffer ({})", hr);
			return false;
		}
		srvOut.Reset();
		hr = srv.As(&srvOut);
		if (FAILED(hr))
		{
			logger::error("Failed to move buffer resource view ({})", hr);
			return false;
		}
		return true;
	};

	bool ObjectNormalMapUpdater::IsValidPixel(const std::uint32_t a_pixel)
	{
		return (a_pixel & 0xFF000000) != 0;
	}
	bool Mus::ObjectNormalMapUpdater::BleedTexture(TextureResourceDataPtr& resourceData, std::int32_t margin, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		if (margin == 0)
			return true;

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
			return false;

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

		HRESULT hr;

		D3D11_TEXTURE2D_DESC stagingDesc = {};
		texInOut->GetDesc(&stagingDesc);
		stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.MiscFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		stagingDesc.MipLevels = 1;
		stagingDesc.ArraySize = 1;
		stagingDesc.SampleDesc.Count = 1;
		hr = device->CreateTexture2D(&stagingDesc, nullptr, &resourceData->bleedTextureData.texture2D_1);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create staging texture ({})", __func__, hr);
			return false;
		}

		CopySubresourceRegion(resourceData->bleedTextureData.texture2D_1, texInOut, 0, 0);

		WaitForGPU();

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		hr = context->Map(resourceData->bleedTextureData.texture2D_1.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mappedResource);
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		std::uint8_t* pData = reinterpret_cast<std::uint8_t*>(mappedResource.pData);

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

		if (margin < 0)
			margin = std::max(UINT(1), std::max(stagingDesc.Width, stagingDesc.Height) / 512);
		for (std::uint32_t mi = 0; mi < margin; mi++)
		{
			std::vector<std::future<void>> processes;
			std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(stagingDesc.Width), processingThreads->GetThreads() * 16));
			std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(stagingDesc.Height), processingThreads->GetThreads() * 16));
			std::size_t unitX = (std::size_t(stagingDesc.Width) + subX - 1) / subX;
			std::size_t unitY = (std::size_t(stagingDesc.Height) + subY - 1) / subY;
			struct ColorMap {
				std::uint32_t* src = nullptr;
				RGBA resultColor;
			};
			concurrency::concurrent_vector<ColorMap> resultColorMap;
			for (std::size_t px = 0; px < subX; px++) {
				for (std::size_t py = 0; py < subY; py++) {
					std::size_t beginX = px * unitX;
					std::size_t endX = std::min(beginX + unitX, std::size_t(stagingDesc.Width));
					std::size_t beginY = py * unitY;
					std::size_t endY = std::min(beginY + unitY, std::size_t(stagingDesc.Height));
					processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
						for (UINT y = beginY; y < endY; y++) {
							std::uint8_t* rowData = pData + y * mappedResource.RowPitch;
							for (UINT x = beginX; x < endX; ++x)
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
										nearCoord.x >= stagingDesc.Width || nearCoord.y >= stagingDesc.Height)
										continue;

									std::uint8_t* nearRowData = pData + nearCoord.y * mappedResource.RowPitch;
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
								resultColorMap.push_back(ColorMap{ pixel, resultColor });
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

			std::size_t sub = std::max(std::size_t(1), std::min(resultColorMap.size(), processingThreads->GetThreads()));
			std::size_t unit = (resultColorMap.size() + sub - 1) / sub;
			for (std::size_t p = 0; p < sub; p++) {
				std::size_t begin = p * unit;
				std::size_t end = std::min(begin + unit, resultColorMap.size());
				processes.push_back(processingThreads->submitAsync([&, begin, end]() {
					for (std::size_t i = begin; i < end; i++) {
						*resultColorMap[i].src = resultColorMap[i].resultColor.GetReverse();
					}
				}));
			}
			for (auto& process : processes)
			{
				process.get();
			}
		}
		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		context->Unmap(resourceData->bleedTextureData.texture2D_1.Get(), 0);
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);

		CopySubresourceRegion(texInOut, resourceData->bleedTextureData.texture2D_1, 0, 0);

		return true;
	}
	bool ObjectNormalMapUpdater::BleedTextureGPU(TextureResourceDataPtr& resourceData, std::int32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		std::string_view _func_ = __func__;
		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, false, false);

		if (margin == 0)
			return true;

		logger::debug("{}::{} : Bleed texture... {}", _func_, resourceData->textureName, margin);

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
		cbData.width = width;
		cbData.height = height;

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
		hr = device->CreateTexture2D(&desc, nullptr, &resourceData->bleedTextureData.texture2D_1);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create texture 2d 1 ({})", _func_, hr);
			return false;
		}
		hr = device->CreateTexture2D(&desc, nullptr, &resourceData->bleedTextureData.texture2D_2);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create texture 2d 2 ({})", _func_, hr);
			return false;
		}

		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		hr = device->CreateShaderResourceView(resourceData->bleedTextureData.texture2D_1.Get(), &srvDesc, &resourceData->bleedTextureData.srv1);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}
		hr = device->CreateShaderResourceView(resourceData->bleedTextureData.texture2D_2.Get(), &srvDesc, &resourceData->bleedTextureData.srv2);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		hr = device->CreateUnorderedAccessView(resourceData->bleedTextureData.texture2D_1.Get(), &uavDesc, &resourceData->bleedTextureData.uav1);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}
		hr = device->CreateUnorderedAccessView(resourceData->bleedTextureData.texture2D_2.Get(), &uavDesc, &resourceData->bleedTextureData.uav2);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}

		if (Config::GetSingleton().GetBleedTextureTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, true, false);

		std::mutex constBuffersLock;
		const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
		const DirectX::XMUINT2 dispatch = { (std::min(width, subResolution) + 8 - 1) / 8, (std::min(height, subResolution) + 8 - 1) / 8};
		if (margin < 0)
			margin = std::max(UINT(1), std::max(width, height) / 512);
		const std::uint32_t marginUnit = std::max(std::uint32_t(1), std::min(std::uint32_t(margin), subResolution / width + subResolution / height));
		const std::uint32_t marginMax = std::max(std::uint32_t(1), margin / marginUnit);
		const std::uint32_t subXSize = std::max(std::uint32_t(1), width / subResolution);
		const std::uint32_t subYSize = std::max(std::uint32_t(1), height / subResolution);

		bool isResultFirst = true;
		for (std::uint32_t mi = 0; mi < marginMax; mi++)
		{
			if (mi > 0)
				isResultFirst = !isResultFirst;
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

						if (Config::GetSingleton().GetBleedTextureTime2())
						GPUPerformanceLog(std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
										  + "::" + std::to_string(subX) + "|" + std::to_string(subY) + "::" + std::to_string(mi), false, false);

						Shader::ShaderManager::GetSingleton().ShaderContextLock();
						sb.Backup(context);
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						if (mi == 0)
						{
							context->CSSetShaderResources(0, 1, srvInOut.GetAddressOf());
							context->CSSetUnorderedAccessViews(0, 1, resourceData->bleedTextureData.uav1.GetAddressOf(), nullptr);
						}
						else
						{
							if (isResultFirst)
							{
								context->CSSetShaderResources(0, 1, resourceData->bleedTextureData.srv2.GetAddressOf());
								context->CSSetUnorderedAccessViews(0, 1, resourceData->bleedTextureData.uav1.GetAddressOf(), nullptr);
							}
							else
							{
								context->CSSetShaderResources(0, 1, resourceData->bleedTextureData.srv1.GetAddressOf());
								context->CSSetUnorderedAccessViews(0, 1, resourceData->bleedTextureData.uav2.GetAddressOf(), nullptr);
							}
						}
						context->Dispatch(dispatch.x, dispatch.y, 1);
						for (std::uint32_t mu = 1; mu < marginUnit; mu++)
						{
							ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
							ID3D11UnorderedAccessView* nullUAV[1] = { nullptr };
							context->CSSetShaderResources(0, 1, nullSRV);
							context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
							isResultFirst = !isResultFirst;
							if (isResultFirst)
							{
								context->CSSetShaderResources(0, 1, resourceData->bleedTextureData.srv2.GetAddressOf());
								context->CSSetUnorderedAccessViews(0, 1, resourceData->bleedTextureData.uav1.GetAddressOf(), nullptr);
							}
							else
							{
								context->CSSetShaderResources(0, 1, resourceData->bleedTextureData.srv1.GetAddressOf());
								context->CSSetUnorderedAccessViews(0, 1, resourceData->bleedTextureData.uav2.GetAddressOf(), nullptr);
							}
							context->Dispatch(dispatch.x, dispatch.y, 1);
						}
						sb.Revert(context);
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
						constBuffersLock.lock();
						resourceData->bleedTextureData.constBuffers.push_back(constBuffer);
						constBuffersLock.unlock();

						if (Config::GetSingleton().GetBleedTextureTime2())
							GPUPerformanceLog(std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
										  + "::" + std::to_string(subX) + "|" + std::to_string(subY) + "::" + std::to_string(mi), true, false);
					}));
				}
			}
			for (auto& task : gpuTasks)
			{
				task.get();
			}
		}
		if (isResultFirst)
		{
			CopySubresourceRegion(texInOut, resourceData->bleedTextureData.texture2D_1, 0, 0);
		}
		else
		{
			CopySubresourceRegion(texInOut, resourceData->bleedTextureData.texture2D_2, 0, 0);
		}

		return true;
	}

	bool ObjectNormalMapUpdater::TexturePostProcessingGPU(TextureResourceDataPtr& resourceData, std::uint32_t blurRadius, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskSrv, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		if (!maskSrv || !srvInOut || !texInOut)
			return false;
		if (blurRadius == 0)
			return true;

		std::string_view _func_ = __func__;
#ifdef POSTPROCESSING_TEST1
		PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);
#endif // POSTPROCESSING_TEST1

		logger::debug("{}::{} : TexturePostProcessing...", _func_, resourceData->textureName);

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(TexturePostProcessingShaderName.data());
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
			UINT width;
			UINT height;
			UINT widthStart;
			UINT heightStart;

			INT searchRadius;
			UINT padding1;
			UINT padding2;
			UINT padding3;
		};
		static_assert(sizeof(ConstBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");

		struct ShaderBackup {
		public:
			void Backup(ID3D11DeviceContext* context) {
				context->CSGetShader(&shader, nullptr, 0);
				context->CSGetConstantBuffers(0, 1, &constBuffer);
				context->CSGetShaderResources(0, 1, &srv);
				context->CSGetUnorderedAccessViews(0, 1, &uav);
				context->CSGetSamplers(0, 1, &samplerState);
			}
			void Revert(ID3D11DeviceContext* context) {
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, srv.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
				context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
			}
			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
			Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
		private:
		} sb;

		const UINT width = desc.Width;
		const UINT height = desc.Height;

		ConstBufferData cbData = {};
		cbData.width = width;
		cbData.height = height;
		cbData.searchRadius = 1024; //std::max(1.0f, (float)blurRadius * ((float)width / 4096.0f));

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
		desc.MipLevels = srvDesc.Texture2D.MipLevels;
		desc.CPUAccessFlags = 0;
		hr = device->CreateTexture2D(&desc, nullptr, &resourceData->texturePostProcessingData.texture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create texture 2d ({})", _func_, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		hr = device->CreateUnorderedAccessView(resourceData->texturePostProcessingData.texture2D.Get(), &uavDesc, &resourceData->texturePostProcessingData.uav);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}
#ifdef POSTPROCESSING_TEST1
		PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, true, false);
#endif // POSTPROCESSING_TEST1

		std::mutex constBuffersLock;
		std::vector<std::future<void>> gpuTasks;
		const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
		const DirectX::XMUINT2 dispatch = { (std::min(width, subResolution) + 8 - 1) / 8, (std::min(height, subResolution) + 8 - 1) / 8 };
		const std::uint32_t subXSize = std::max(std::uint32_t(1), width / subResolution);
		const std::uint32_t subYSize = std::max(std::uint32_t(1), height / subResolution);
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
				gpuTasks.push_back(gpuTask->submitAsync([&, constBuffer]() {
#ifdef POSTPROCESSING_TEST2
					GPUPerformanceLog(std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height), false, false);
#endif // POSTPROCESSING_TEST2
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					sb.Backup(context);
					context->CSSetShader(shader.Get(), nullptr, 0);
					context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
					context->CSSetShaderResources(0, 1, srvInOut.GetAddressOf());
					context->CSSetShaderResources(1, 1, maskSrv.GetAddressOf());
					context->CSSetUnorderedAccessViews(0, 1, resourceData->texturePostProcessingData.uav.GetAddressOf(), nullptr);
					context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
					context->Dispatch(dispatch.x, dispatch.y, 1);
					sb.Revert(context);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					resourceData->texturePostProcessingData.constBuffer.push_back(constBuffer);
#ifdef POSTPROCESSING_TEST2
					GPUPerformanceLog(std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height), true, false);
#endif // POSTPROCESSING_TEST2
				}));
			}
		}
		for (auto& task : gpuTasks)
		{
			task.get();
		}

		CopySubresourceRegion(texInOut, resourceData->texturePostProcessingData.texture2D, 0, 0);
		return true;
	}

	bool ObjectNormalMapUpdater::MergeTexture(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11Texture2D> srvTex)
	{
		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
			return false;

		HRESULT hr;

		D3D11_TEXTURE2D_DESC stagingDesc = {}, stagingDescAlt = {};
		dstTex->GetDesc(&stagingDesc);
		stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.MiscFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		stagingDesc.MipLevels = 1;
		stagingDesc.ArraySize = 1;
		stagingDesc.SampleDesc.Count = 1;
		srvTex->GetDesc(&stagingDescAlt);
		stagingDescAlt.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		stagingDescAlt.Usage = D3D11_USAGE_STAGING;
		stagingDescAlt.BindFlags = 0;
		stagingDescAlt.MiscFlags = 0;
		stagingDescAlt.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		stagingDescAlt.MipLevels = 1;
		stagingDescAlt.ArraySize = 1;
		stagingDescAlt.SampleDesc.Count = 1;
		hr = device->CreateTexture2D(&stagingDesc, nullptr, &resourceData->mergeTextureData.texture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create staging texture ({})", __func__, hr);
			return false;
		}
		hr = device->CreateTexture2D(&stagingDescAlt, nullptr, &resourceData->mergeTextureData.texture2Dalt);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create staging texture ({})", __func__, hr);
			return false;
		}

		CopySubresourceRegion(resourceData->mergeTextureData.texture2D, dstTex, 0, 0);
		CopySubresourceRegion(resourceData->mergeTextureData.texture2Dalt, srvTex, 0, 0);

		WaitForGPU();

		D3D11_MAPPED_SUBRESOURCE mappedResource, mappedResourceAlt;
		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		hr = context->Map(resourceData->mergeTextureData.texture2D.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mappedResource);
		hr = context->Map(resourceData->mergeTextureData.texture2Dalt.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mappedResourceAlt);
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

		std::uint8_t* pData = reinterpret_cast<std::uint8_t*>(mappedResource.pData);
		std::uint8_t* pDataAlt = reinterpret_cast<std::uint8_t*>(mappedResourceAlt.pData);
		std::vector<std::future<void>> processes;
		std::size_t subX = std::max((std::size_t)1, std::min(std::size_t(stagingDesc.Width), processingThreads->GetThreads() * 8));
		std::size_t subY = std::max((std::size_t)1, std::min(std::size_t(stagingDesc.Height), processingThreads->GetThreads() * 8));
		std::size_t unitX = (std::size_t(stagingDesc.Width) + subX - 1) / subX;
		std::size_t unitY = (std::size_t(stagingDesc.Height) + subY - 1) / subY;
		for (std::size_t px = 0; px < subX; px++) {
			for (std::size_t py = 0; py < subY; py++) {
				std::size_t beginX = px * unitX;
				std::size_t endX = std::min(beginX + unitX, std::size_t(stagingDesc.Width));
				std::size_t beginY = py * unitY;
				std::size_t endY = std::min(beginY + unitY, std::size_t(stagingDesc.Height));
				processes.push_back(processingThreads->submitAsync([&, beginX, endX, beginY, endY]() {
					for (UINT y = beginY; y < endY; y++) {
						std::uint8_t* rowData = pData + y * mappedResource.RowPitch;
						UINT my = std::min(stagingDescAlt.Height, stagingDescAlt.Height * (y / stagingDesc.Height));
						std::uint8_t* rowDataAlt = pDataAlt + my * mappedResourceAlt.RowPitch;
						for (UINT x = beginX; x < endX; ++x)
						{
							std::uint32_t* pixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
							UINT mx = std::min(stagingDescAlt.Width, stagingDescAlt.Width * (x / stagingDesc.Width));
							std::uint32_t* pixelAlt = reinterpret_cast<uint32_t*>(rowDataAlt + mx * 4);
							RGBA pixelColor, pixelColorAlt;
							pixelColor.SetReverse(*pixel);
							pixelColorAlt.SetReverse(*pixelAlt);
							if (pixelColor.a < 1.0f && pixelColorAlt.a < 1.0f)
								continue;

							RGBA dstColor = RGBA::lerp(pixelColorAlt, pixelColor, pixelColor.a);
							*pixel = dstColor.GetReverse() | 0xFF000000;
						}
					}
				}));
			}
		}
		for (auto& process : processes)
		{
			process.get();
		}

		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		context->Unmap(resourceData->mergeTextureData.texture2D.Get(), 0);
		context->Unmap(resourceData->mergeTextureData.texture2Dalt.Get(), 0);
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);

		CopySubresourceRegion(dstTex, resourceData->mergeTextureData.texture2D, 0, 0);
		return true;
	}
	bool ObjectNormalMapUpdater::MergeTextureGPU(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D> srvTex)
	{
		std::string_view _func_ = __func__;
		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, false, false);

		logger::debug("{}::{} : Merge texture...", _func_, resourceData->textureName);

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		auto shader = Shader::ShaderManager::GetSingleton().GetComputeShader(MergeTextureShaderName.data());
		if (!device || !context || !shader)
			return false;

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
			}
			void Revert(ID3D11DeviceContext* context) {
				context->CSSetShader(shader.Get(), nullptr, 0);
				context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
				context->CSSetShaderResources(0, 1, srv1.GetAddressOf());
				context->CSSetShaderResources(1, 1, srv2.GetAddressOf());
				context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
			}
			Shader::ShaderManager::ComputeShader shader;
			Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv1;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv2;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		private:
		} sb;


		const UINT width = desc.Width;
		const UINT height = desc.Height;

		ConstBufferData cbData = {};
		cbData.width = width;
		cbData.height = height;

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
		hr = device->CreateTexture2D(&desc, nullptr, &resourceData->mergeTextureData.texture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create texture 2d ({})", _func_, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		hr = device->CreateUnorderedAccessView(resourceData->mergeTextureData.texture2D.Get(), &uavDesc, &resourceData->mergeTextureData.uav);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}

		if (Config::GetSingleton().GetMergeTime1())
			PerformanceLog(std::string(_func_) + "::" + resourceData->textureName, true, false);

		std::mutex constBuffersLock;
		const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
		const DirectX::XMUINT2 dispatch = { (std::min(width, subResolution) + 8 - 1) / 8, (std::min(height, subResolution) + 8 - 1) / 8 };
		const std::uint32_t subXSize = std::max(std::uint32_t(1), width / subResolution);
		const std::uint32_t subYSize = std::max(std::uint32_t(1), height / subResolution);

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
						GPUPerformanceLog(std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
									  + "::" + std::to_string(subX) + "|" + std::to_string(subY), false, false);

					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					sb.Backup(context);
					context->CSSetShader(shader.Get(), nullptr, 0);
					context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
					context->CSSetShaderResources(0, 1, srcSrv.GetAddressOf());
					context->CSSetShaderResources(1, 1, dstSrv.GetAddressOf());
					context->CSSetUnorderedAccessViews(0, 1, resourceData->mergeTextureData.uav.GetAddressOf(), nullptr);
					context->Dispatch(dispatch.x, dispatch.y, 1);
					sb.Revert(context);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					constBuffersLock.lock();
					resourceData->mergeTextureData.constBuffers.push_back(constBuffer);
					constBuffersLock.unlock();

					if (Config::GetSingleton().GetMergeTime2())
						GPUPerformanceLog(std::string(_func_) + "::" + resourceData->textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
									  + "::" + std::to_string(subX) + "|" + std::to_string(subY), true, false);
				}));
			}
		}
		for (auto& task : gpuTasks)
		{
			task.get();
		}

		CopySubresourceRegion(dstTex, resourceData->mergeTextureData.texture2D, 0, 0);
		return true;
	}

	bool ObjectNormalMapUpdater::CopySubresourceRegion(Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, UINT dstMipMapLevel, UINT srcMipMapLevel)
	{
		D3D11_TEXTURE2D_DESC dstDesc, srcDesc;
		dstTexture->GetDesc(&dstDesc);
		srcTexture->GetDesc(&srcDesc);

		if (dstDesc.Width != srcDesc.Width || dstDesc.Height != srcDesc.Height)
			return false;

		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!context)
			return false;

		const UINT width = dstDesc.Width;
		const UINT height = dstDesc.Height;
		const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
		const std::uint32_t subXSize = std::max(std::uint32_t(1), width / subResolution);
		const std::uint32_t subYSize = std::max(std::uint32_t(1), height / subResolution);
		std::vector<std::future<void>> gpuTasks;
		for (std::uint32_t subY = 0; subY < subYSize; subY++)
		{
			for (std::uint32_t subX = 0; subX < subXSize; subX++)
			{
				D3D11_BOX box = {};
				box.left = subX * subResolution;
				box.right = std::min(width, box.left + subResolution);
				box.top = subY * subResolution;
				box.bottom = std::min(height, box.top + subResolution);
				box.front = 0;
				box.back = 1;

				gpuTasks.push_back(gpuTask->submitAsync([&, box, subX, subY]() {
					if (Config::GetSingleton().GetTextureCopyTime())
						GPUPerformanceLog(std::string("CopySubresourceRegion") + "::" + std::to_string(dstDesc.Width) + "::" + std::to_string(dstDesc.Width)
									  + "::" + std::to_string(subX) + "|" + std::to_string(subY), false, false);

					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					context->CopySubresourceRegion(
						dstTexture.Get(), dstMipMapLevel,
						box.left, box.top, 0,
						srcTexture.Get(), srcMipMapLevel,
						&box);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock(); 

					if (Config::GetSingleton().GetTextureCopyTime())
						GPUPerformanceLog(std::string("CopySubresourceRegion") + "::" + std::to_string(dstDesc.Width) + "::" + std::to_string(dstDesc.Width)
									  + "::" + std::to_string(subX) + "|" + std::to_string(subY), true, false);
				}));
			}
		}
		for (auto& task : gpuTasks) {
			task.get();
		}
		return true;
	}

	bool ObjectNormalMapUpdater::CompressTexture(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srcSrv)
	{
		if (!srcTexture || !srcSrv || Config::GetSingleton().GetTextureCompress() == 0)
			return false;

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		if (!device)
			return false;

		bool isCompressed = false;
		DXGI_FORMAT format = DXGI_FORMAT_BC7_UNORM;
		if (Config::GetSingleton().GetTextureCompress() == 1)
			format = DXGI_FORMAT_BC3_UNORM;
		else if (Config::GetSingleton().GetTextureCompress() == 2)
			format = DXGI_FORMAT_BC7_UNORM;
		else
			return false;


		gpuTask->submitAsync([&]() {
			if (Config::GetSingleton().GetCompressTime())
				PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, false, false);

			isCompressed = Shader::TextureLoadManager::GetSingleton().CompressTexture(srcTexture, format, dstTexture);

			if (Config::GetSingleton().GetCompressTime())
				PerformanceLog(std::string(__func__) + "::" + resourceData->textureName, true, false);
		}).get();
		//isCompressed = CompressTextureBC7(resourceData, dstTexture, dstSrv, srcTexture, srcSrv);


		if (isCompressed)
		{
			D3D11_TEXTURE2D_DESC desc;
			dstTexture->GetDesc(&desc);
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srcSrv->GetDesc(&srvDesc);
			srvDesc.Format = desc.Format;
			srvDesc.Texture2D.MipLevels = desc.MipLevels;
			auto hr = device->CreateShaderResourceView(dstTexture.Get(), &srvDesc, &dstSrv);
			if (FAILED(hr)) {
				logger::error("{}::{} : Failed to create ShaderResourceView ({})", __func__, resourceData->textureName, hr);
				return false;
			}
		}

		return isCompressed;
	}

	bool ObjectNormalMapUpdater::CompressTextureBC7(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srcSrv)
	{
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();

		HRESULT hr;

		D3D11_TEXTURE2D_DESC srcDesc{};
		srcTexture->GetDesc(&srcDesc);
		D3D11_SHADER_RESOURCE_VIEW_DESC srcSrvDesc{};
		srcSrv->GetDesc(&srcSrvDesc);

		D3D11_TEXTURE2D_DESC dstDesc = srcDesc;
		dstDesc.Format = DXGI_FORMAT_BC7_UNORM;
		dstDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		dstDesc.MipLevels = 1;
		dstDesc.CPUAccessFlags = 0;
		dstDesc.Usage = D3D11_USAGE_DEFAULT;
		dstDesc.MiscFlags = 0;

		std::vector<std::vector<std::uint8_t>> levelBuffers(dstDesc.MipLevels);
		std::vector<std::size_t> levelSizes(dstDesc.MipLevels);

		bc7enc_compress_block_init();

		bc7enc_compress_block_params params;
		bc7enc_compress_block_params_init(&params);
		bc7enc_compress_block_params_init_linear_weights(&params);
		params.m_mode_mask = 0xFF;

		for (UINT level = 0; level < dstDesc.MipLevels; ++level)
		{
			UINT w = std::max(1u, dstDesc.Width >> level);
			UINT h = std::max(1u, dstDesc.Height >> level);
			UINT bw = (w + 3) / 4;
			UINT bh = (h + 3) / 4;
			size_t blocks = bw * bh;
			levelSizes[level] = blocks * 16;
			levelBuffers[level].resize(levelSizes[level]);

			D3D11_TEXTURE2D_DESC stagingDesc = dstDesc;
			stagingDesc.Width = w;
			stagingDesc.Height = h;
			stagingDesc.MipLevels = 1;
			stagingDesc.ArraySize = 1;
			stagingDesc.BindFlags = 0;
			stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			stagingDesc.Usage = D3D11_USAGE_STAGING;
			stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
			hr = device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to map staging texture for level {}", level);
				return false;
			}

			CopySubresourceRegion(staging, srcTexture, 0, level);

			WaitForGPU();

			D3D11_MAPPED_SUBRESOURCE mapped;
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			hr = context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to map staging texture at level {}", level);
				return false;
			}

			std::uint8_t* srcData = reinterpret_cast<uint8_t*>(mapped.pData);

			std::vector<std::future<void>> processes;
			std::size_t blockIndex = 0;
			for (UINT by = 0; by < bh; by++)
			{
				for (UINT bx = 0; bx < bw; bx++)
				{
					const std::size_t currentBlockIndex = by * bw + bx;
					processes.push_back(processingThreads->submitAsync([&, bx, by, currentBlockIndex] {
						std::uint8_t block[64] = {};
						for (UINT y = 0; y < 4; y++)
						{
							for (UINT x = 0; x < 4; x++)
							{
								UINT px = bx * 4 + x;
								UINT py = by * 4 + y;
								UINT clampedPx = std::min(px, w - 1);
								UINT clampedPy = std::min(py, h - 1);
								std::uint32_t* dstPixel = reinterpret_cast<uint32_t*>(srcData + clampedPy * mapped.RowPitch + clampedPx * 4);
								RGBA rgba;
								rgba.SetReverse(*dstPixel);
								std::uint8_t* dst = block + (y * 4 + x) * 4;
								dst[0] = rgba.r; 
								dst[1] = rgba.g; 
								dst[2] = rgba.b; 
								dst[3] = rgba.a;
							}
						}
						std::uint8_t* outputPtr = levelBuffers[level].data() + currentBlockIndex * 16;
						bc7enc_compress_block(outputPtr, block, &params);
					}));
				}
			}
			for (auto& process : processes) {
				process.get();
			}
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->Unmap(staging.Get(), 0);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		}

		std::vector<D3D11_SUBRESOURCE_DATA> initData(dstDesc.MipLevels);
		for (UINT level = 0; level < dstDesc.MipLevels; level++)
		{
			UINT w = std::max(1u, dstDesc.Width >> level);
			initData[level].pSysMem = levelBuffers[level].data();
			initData[level].SysMemPitch = ((w + 3) / 4) * 16;
			initData[level].SysMemSlicePitch = 0;
		}

		hr = device->CreateTexture2D(&dstDesc, initData.data(), dstTexture.GetAddressOf());
		if (FAILED(hr))
		{
			logger::error("Failed to create BC7 compressed");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = dstDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = dstDesc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		hr = device->CreateShaderResourceView(dstTexture.Get(), &srvDesc, dstSrv.GetAddressOf());
		if (FAILED(hr))
		{
			logger::error("Failed to create shader resource view for BC7");
			return false;
		}
		return true;
	}

	void ObjectNormalMapUpdater::GPUPerformanceLog(std::string funcStr, bool isEnd, bool isAverage, std::uint32_t args)
	{
		if (!PerformanceCheck)
			return;

		struct GPUTimer
		{
			Microsoft::WRL::ComPtr<ID3D11Query> startQuery;
			Microsoft::WRL::ComPtr<ID3D11Query> endQuery;
			Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
		};

		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
			return;

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

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->Begin(gpuTimers[funcStr].disjointQuery.Get());
			context->End(gpuTimers[funcStr].startQuery.Get());
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		}
		else
		{
			double tick = PerformanceCheckTick ? (double)(RE::GetSecondsSinceLastFrame() * 1000) : (double)(TimeTick60 * 1000);
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->End(gpuTimers[funcStr].endQuery.Get());
			context->End(gpuTimers[funcStr].disjointQuery.Get());

			context->Flush();
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData = {};
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			while (context->GetData(gpuTimers[funcStr].disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) != S_OK);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			if (disjointData.Disjoint)
				return;

			UINT64 startTime = 0, endTime = 0;
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			while (context->GetData(gpuTimers[funcStr].startQuery.Get(), &startTime, sizeof(startTime), 0) != S_OK);
			while (context->GetData(gpuTimers[funcStr].endQuery.Get(), &endTime, sizeof(endTime), 0) != S_OK);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			double duration_ms = (double)(endTime - startTime) / disjointData.Frequency * 1000.0;

			if (isAverage) {
				funcAverageArgs[funcStr] += duration_ms;
				funcAverageCount[funcStr]++;

				if (funcAverageCount[funcStr] >= 60) {
					double average = funcAverageArgs[funcStr] / funcAverageCount[funcStr];
					logger::info("{} average time: {:.6f}ms{}=> {:.6f}%", funcStr, average,
								 funcAverageArgs[funcStr] > 0 ? (std::string(" with average count ") + std::to_string(funcAverageArgs[funcStr] / funcAverageCount[funcStr]) + " ") : " ",
								 (double)average / tick * 100);
					if (PerformanceCheckConsolePrint) {
						auto Console = RE::ConsoleLog::GetSingleton();
						if (Console)
							Console->Print("%s average time: %lldms%s=> %.6f%%", funcStr.c_str(), average,
										   funcAverageArgs[funcStr] > 0 ? (std::string(" with average count ") + std::to_string(funcAverageArgs[funcStr] / funcAverageCount[funcStr]) + " ").c_str() : " ",
										   (double)average / tick * 100);
					}
					funcAverageArgs[funcStr] = 0;
					funcAverageCount[funcStr] = 0;
				}
			}
			else {
				logger::info("{} time: {:.6f}ms{}=> {:.6f}%", funcStr, duration_ms,
							 args > 0 ? (std::string(" with count ") + std::to_string(args) + " ") : " ",
							 (double)duration_ms / tick * 100
				);
				if (PerformanceCheckConsolePrint) {
					auto Console = RE::ConsoleLog::GetSingleton();
					if (Console)
						Console->Print("%s time: %lld ms%s=> %.6f%%", funcStr.c_str(), duration_ms,
									   args > 0 ? (std::string(" with count ") + std::to_string(args) + " ").c_str() : " ",
									   (double)duration_ms / tick * 100);
				}
			}
		}
	}

	void ObjectNormalMapUpdater::WaitForGPU()
	{
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
			return;

		D3D11_QUERY_DESC queryDesc = {};
		queryDesc.Query = D3D11_QUERY_EVENT;
		queryDesc.MiscFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Query> query;
		HRESULT hr = device->CreateQuery(&queryDesc, &query);
		if (FAILED(hr)) {
			return;
		}
		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		context->End(query.Get());
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
		std::uint32_t spinCount = 0;
		logger::debug("Wait for Renderer...");
		while (true) {
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			hr = context->GetData(query.Get(), nullptr, 0, 0);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
				break;
			if (hr == S_OK)
				break;
			if (spinCount < 100)
				std::this_thread::yield();
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			spinCount++;
		}
		logger::debug("Wait for Renderer done");
		return;
	}
}
