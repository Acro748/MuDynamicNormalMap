#include "ObjectNormalMapUpdater.h"

namespace Mus {
//#define BAKE_TEST1
//#define BAKE_TEST2
//#define BLEED_TEST1
//#define BLEED_TEST2

	void ObjectNormalMapUpdater::onEvent(const FrameEvent& e)
	{
		ResourceDataMapLock.lock();
		auto ResourceDataMap_ = ResourceDataMap;
		ResourceDataMap.clear();
		ResourceDataMapLock.unlock();
		processingThreads->submitAsync([&, ResourceDataMap_]() {
			for (auto map : ResourceDataMap_)
			{
				if (map.IsQueryDone(Shader::ShaderManager::GetSingleton().GetContext()))
					continue;

				ResourceDataMapLock.lock_shared();
				ResourceDataMap.push_back(map);
				ResourceDataMapLock.unlock_shared();

			}
		});
		processingThreads->submitAsync([&]() {
			for (auto& map : GeometryResourceDataMap)
			{
				if (!map.second.query)
					continue;
				if (map.second.IsQueryDone(Shader::ShaderManager::GetSingleton().GetContext()))
					map.second.clear();
			}
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
	}

	bool ObjectNormalMapUpdater::CreateGeometryResourceData(RE::FormID a_actorID, GeometryData& a_data)
	{
		a_data.GetGeometryData();

		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| a_data.geometries.empty() || a_actorID == 0)
		{
			logger::error("{} : Invalid parameters", __func__);
			return false;
		}
		a_data.UpdateMap();
		//a_data.Subdivision(Config::GetSingleton().GetSubdivision());
		//a_data.VertexSmooth(Config::GetSingleton().GetVertexSmoothStrength(), Config::GetSingleton().GetVertexSmooth());
		a_data.RecalculateNormals(Config::GetSingleton().GetNormalSmoothDegree());

		if (a_data.vertices.size() != a_data.uvs.size() ||
			a_data.vertices.size() != a_data.normals.size() ||
			a_data.vertices.size() != a_data.tangents.size() ||
			a_data.vertices.size() != a_data.bitangents.size())
		{
			logger::error("{} : Invalid geometry", __func__);
			return false;
		}

		if (Config::GetSingleton().GetGPUEnable())
		{
			GeometryResourceDataMap[a_actorID].query.Reset();
			if (!CreateStructuredBuffer(a_data.vertices.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data.vertices.size()), sizeof(DirectX::XMFLOAT3), GeometryResourceDataMap[a_actorID].vertexBuffer, GeometryResourceDataMap[a_actorID].vertexSRV))
				return false;
			if (!CreateStructuredBuffer(a_data.uvs.data(), UINT(sizeof(DirectX::XMFLOAT2) * a_data.uvs.size()), sizeof(DirectX::XMFLOAT2), GeometryResourceDataMap[a_actorID].uvBuffer, GeometryResourceDataMap[a_actorID].uvSRV))
				return false;
			if (!CreateStructuredBuffer(a_data.normals.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data.normals.size()), sizeof(DirectX::XMFLOAT3), GeometryResourceDataMap[a_actorID].normalBuffer, GeometryResourceDataMap[a_actorID].normalSRV))
				return false;
			if (!CreateStructuredBuffer(a_data.tangents.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data.tangents.size()), sizeof(DirectX::XMFLOAT3), GeometryResourceDataMap[a_actorID].tangentBuffer, GeometryResourceDataMap[a_actorID].tangentSRV))
				return false;
			if (!CreateStructuredBuffer(a_data.bitangents.data(), UINT(sizeof(DirectX::XMFLOAT3) * a_data.bitangents.size()), sizeof(DirectX::XMFLOAT3), GeometryResourceDataMap[a_actorID].bitangentBuffer, GeometryResourceDataMap[a_actorID].bitangentSRV))
				return false;
			if (!CreateStructuredBuffer(a_data.indices.data(), UINT(sizeof(std::uint32_t) * a_data.indices.size()), sizeof(std::uint32_t), GeometryResourceDataMap[a_actorID].indicesBuffer, GeometryResourceDataMap[a_actorID].indicesSRV))
				return false;
		}
		return true;
	}

	ObjectNormalMapUpdater::BakeResult ObjectNormalMapUpdater::UpdateObjectNormalMap(RE::FormID a_actorID, GeometryData& a_data, BakeSet& a_bakeSet)
	{
		std::string_view _func_ = __func__;
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID), false, false);
#endif // BAKE_TEST1

		BakeResult result;

		HRESULT hr;
		auto device = Shader::ShaderManager::GetSingleton().GetDevice();
		auto context = Shader::ShaderManager::GetSingleton().GetContext();
		if (!device || !context)
		{
			logger::error("{}::{:x} : Invalid renderer", _func_, a_actorID);
			return result;
		}

		const std::int32_t margin = Config::GetSingleton().GetTextureMargin();
		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_bakeSet.size());

		for (auto& bake : a_bakeSet)
		{
			auto found = std::find_if(a_data.geometries.begin(), a_data.geometries.end(), [&](std::pair<RE::BSGeometry*, GeometryData::ObjectInfo>& geometry) {
				return geometry.first == bake.first;
									  });
			if (found == a_data.geometries.end())
			{
				logger::error("{}::{:x} : Geometry {} not found in data", _func_, a_actorID, bake.second.geometryName);
				continue;
			}
			GeometryData::ObjectInfo& objInfo = found->second;
			TextureResourceData newResourceData;
			newResourceData.textureName = bake.second.textureName;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> srcStagingTexture2D, detailStagingTexture2D, overlayStagingTexture2D, maskStagingTexture2D, dstStagingTexture2D;
			D3D11_TEXTURE2D_DESC srcStagingDesc = {}, detailStagingDesc = {}, overlayStagingDesc = {}, maskStagingDesc = {}, dstStagingDesc = {}, dstDesc = {};
			D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};

			if (!bake.second.srcTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
				D3D11_TEXTURE2D_DESC srcDesc;
				D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
				if (!IsDetailNormalMap(bake.second.srcTexturePath))
				{
					logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.srcTexturePath);

					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.srcTexturePath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM, srcTexture2D))
					{
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
							logger::error("{}::{:x} : Failed to create src staging texture ({}|{})", _func_, a_actorID, hr, bake.second.srcTexturePath);
							srcStagingTexture2D = nullptr;
						}
						else
						{
							Shader::ShaderManager::GetSingleton().ShaderContextLock();
							context->CopyResource(srcStagingTexture2D.Get(), srcTexture2D.Get());
							Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
						}
					}
				}
			}
			if (!bake.second.detailTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> detailTexture2D = nullptr;
				D3D11_TEXTURE2D_DESC detailDesc;
				D3D11_SHADER_RESOURCE_VIEW_DESC detailShaderResourceViewDesc;
				logger::info("{}::{:x}::{} : {} detail texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.detailTexturePath);

				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.detailTexturePath, detailDesc, detailShaderResourceViewDesc, DXGI_FORMAT_R8G8B8A8_UNORM, detailTexture2D))
				{
					auto tmpDesc = dstDesc;
					dstDesc = detailDesc;
					if ((std::uint64_t)detailDesc.Width * (std::uint64_t)detailDesc.Height < (std::uint64_t)tmpDesc.Width * (std::uint64_t)tmpDesc.Height)
					{
						dstDesc.Width = tmpDesc.Width;
						dstDesc.Height = tmpDesc.Height;
					}

					detailStagingDesc = detailDesc;
					detailStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					detailStagingDesc.Usage = D3D11_USAGE_STAGING;
					detailStagingDesc.BindFlags = 0;
					detailStagingDesc.MiscFlags = 0;
					detailStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

					dstShaderResourceViewDesc = detailShaderResourceViewDesc;
					hr = device->CreateTexture2D(&detailStagingDesc, nullptr, &detailStagingTexture2D);
					if (FAILED(hr))
					{
						logger::error("{}::{:x} : Failed to create detail staging texture ({}|{})", _func_, a_actorID, hr, bake.second.detailTexturePath);
						detailStagingTexture2D = nullptr;
					}
					else
					{
						Shader::ShaderManager::GetSingleton().ShaderContextLock();
						context->CopyResource(detailStagingTexture2D.Get(), detailTexture2D.Get());
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					}
				}

			}
			if (!srcStagingTexture2D && !detailStagingTexture2D)
			{
				logger::error("{}::{:x}::{} : There is no Normalmap!", _func_, a_actorID, bake.second.geometryName);
				continue;
			}

			if (!bake.second.overlayTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> overlayTexture2D = nullptr;
				logger::info("{}::{:x}::{} : {} overlay texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.overlayTexturePath);
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
						logger::error("{}::{:x}::{} : Failed to create overlay staging texture ({})", _func_, a_actorID, bake.second.geometryName, hr);
						overlayStagingTexture2D = nullptr;
					}
					else
					{
						Shader::ShaderManager::GetSingleton().ShaderContextLock();
						context->CopyResource(overlayStagingTexture2D.Get(), overlayTexture2D.Get());
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					}
				}
			}

			if (!bake.second.maskTexturePath.empty())
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> maskTexture2D = nullptr;
				logger::info("{}::{:x}::{} : {} mask texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.maskTexturePath);
				D3D11_TEXTURE2D_DESC maskDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.maskTexturePath, maskDesc, DXGI_FORMAT_R8G8B8A8_UNORM, maskTexture2D))
				{
					maskStagingDesc = maskDesc;
					maskStagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					maskStagingDesc.Usage = D3D11_USAGE_STAGING;
					maskStagingDesc.BindFlags = 0;
					maskStagingDesc.MiscFlags = 0;
					maskStagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
					hr = device->CreateTexture2D(&maskStagingDesc, nullptr, &maskStagingTexture2D);
					if (FAILED(hr))
					{
						logger::error("{}::{:x}::{} : Failed to create mask staging texture ({})", _func_, a_actorID, bake.second.geometryName, hr);
						maskStagingTexture2D = nullptr;
					}
					else
					{
						Shader::ShaderManager::GetSingleton().ShaderContextLock();
						context->CopyResource(maskStagingTexture2D.Get(), maskTexture2D.Get());
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
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
				logger::error("{}::{:x}::{} : Failed to create dst staging texture ({})", _func_, a_actorID, bake.second.geometryName, hr);
				continue;
			}

			logger::info("{}::{:x}::{} : {} {} {} {} baking normalmap...", _func_, a_actorID, bake.second.geometryName,
						 objInfo.vertexCount(),
						 objInfo.uvCount(),
						 objInfo.normalCount(),
						 objInfo.indicesCount());

			WaitForGPU();

			D3D11_MAPPED_SUBRESOURCE srcMappedResource;
			uint8_t* srcData = nullptr;
			if (srcStagingTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(srcStagingTexture2D.Get(), 0, D3D11_MAP_READ, 0, &srcMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				srcData = reinterpret_cast<uint8_t*>(srcMappedResource.pData);
			}
			D3D11_MAPPED_SUBRESOURCE detailMappedResource;
			uint8_t* detailData = nullptr;
			if (detailStagingTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(detailStagingTexture2D.Get(), 0, D3D11_MAP_READ, 0, &detailMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				detailData = reinterpret_cast<uint8_t*>(detailMappedResource.pData);
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
			D3D11_MAPPED_SUBRESOURCE maskMappedResource;
			uint8_t* maskData = nullptr;
			if (maskStagingTexture2D)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				hr = context->Map(maskStagingTexture2D.Get(), 0, D3D11_MAP_READ, 0, &maskMappedResource);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				maskData = reinterpret_cast<uint8_t*>(maskMappedResource.pData);
			}

			const bool hasSrcData = (srcData != nullptr);
			const bool hasDetailData = (detailData != nullptr);
			const bool hasOverlayData = (overlayData != nullptr);
			const bool hasMaskData = (maskData != nullptr);

			D3D11_MAPPED_SUBRESOURCE mappedResource;
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			hr = context->Map(dstStagingTexture2D.Get(), 0, D3D11_MAP_READ_WRITE, 0, &mappedResource);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
			{
				logger::error("{}::{:x}::{} Failed to read data from the staging texture ({})", _func_, a_actorID, bake.second.geometryName, hr);
				continue;
			}

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
			const float detailStrength = bake.second.detailStrength;

			std::vector<std::future<void>> processes;
#ifdef BAKE_TEST2
			PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + bake.second.geometryName, false, false);
#endif // BAKE_TEST2
			for (std::size_t i = 0; i < totalTris; i++)
			{
				processes.push_back(processingThreads->submitAsync([&, i]() {
					const std::uint32_t index = objInfo.indicesStart + i * 3;

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

									const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(
										DirectX::XMVectorAdd(DirectX::XMVectorAdd(
											DirectX::XMVectorScale(n0v, bary.x),
											DirectX::XMVectorScale(n1v, bary.y)),
											DirectX::XMVectorScale(n2v, bary.z)));

									DirectX::XMVECTOR normalResult = DirectX::XMVectorZero();
									if (detailColor.a > 0.0f)
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

										const DirectX::XMVECTOR ft = DirectX::XMVector3Normalize(
											DirectX::XMVectorSubtract(t, DirectX::XMVectorScale(n, DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, t)))));

										const DirectX::XMVECTOR cross = DirectX::XMVector3Cross(n, ft);
										const float handedness = (DirectX::XMVectorGetX(DirectX::XMVector3Dot(cross, b)) < 0.0f) ? -1.0f : 1.0f;
										const DirectX::XMVECTOR fb = DirectX::XMVector3Normalize(DirectX::XMVectorScale(cross, handedness));
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
				}));
			}
			for (auto& process : processes)
			{
				process.get();
			}
#ifdef BAKE_TEST2
			PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + bake.second.geometryName, true, false);
#endif // BAKE_TEST2
			if (!Config::GetSingleton().GetTextureMarginGPU() || Shader::ShaderManager::GetSingleton().IsFailedShader(BleedTextureShaderName.data()))
			{
				BleedTexture(dstData, dstStagingDesc.Width, dstStagingDesc.Height, mappedResource.RowPitch, margin);
			}

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->Unmap(dstStagingTexture2D.Get(), 0);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			if (hasSrcData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(srcStagingTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
			if (hasDetailData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(detailStagingTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
			if (hasOverlayData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(overlayStagingTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			}
			if (hasMaskData)
			{
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->Unmap(maskStagingTexture2D.Get(), 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
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
				logger::error("{}::{:x} : Failed to create dst texture ({})", _func_, a_actorID, hr);
				continue;
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
				logger::error("{}::{:x} : Failed to create ShaderResourceView ({})", _func_, a_actorID, hr);
				continue;
			}

			if (Config::GetSingleton().GetTextureMarginGPU() && !Shader::ShaderManager::GetSingleton().IsFailedShader(BleedTextureShaderName.data()))
				BleedTextureGPU(newResourceData, margin, dstShaderResourceView, dstTexture2D);

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->GenerateMips(dstShaderResourceView.Get());
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			newResourceData.GetQuery(device, context);

			RE::NiPointer<RE::NiSourceTexture> output = nullptr;
			bool texCreated = false;
			Shader::TextureLoadManager::GetSingleton().CreateNiTexture(bake.second.textureName, bake.second.detailTexturePath.empty() ? "None" : bake.second.detailTexturePath, dstTexture2D, dstShaderResourceView, output, texCreated);
			if (!output)
				continue;

			NormalMapResult newNormalMapResult;
			newNormalMapResult.geometry = bake.first;
			newNormalMapResult.vertexCount = objInfo.vertexCount();
			newNormalMapResult.geoName = bake.second.geometryName;
			newNormalMapResult.textureName = bake.second.textureName;
			newNormalMapResult.normalmap = output;
			result.push_back(newNormalMapResult);

			ResourceDataMapLock.lock_shared();
			ResourceDataMap.push_back(newResourceData);
			ResourceDataMapLock.unlock_shared();
			logger::info("{}::{:x}::{} : normalmap baked", _func_, a_actorID, bake.second.geometryName);
		}
		GeometryResourceDataMap[a_actorID].GetQuery(device, context);
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID), true, false);
#endif // BAKE_TEST1
		return result;
	}

	ObjectNormalMapUpdater::BakeResult ObjectNormalMapUpdater::UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryData& a_data, BakeSet& a_bakeSet)
	{
		std::string_view _func_ = __func__;
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID), false, false);
#endif // BAKE_TEST1

		BakeResult result;
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

		const std::uint32_t margin = Config::GetSingleton().GetTextureMargin();

		logger::debug("{}::{:x} : updating... {}", _func_, a_actorID, a_bakeSet.size());

		for (auto& bake : a_bakeSet)
		{
			auto found = std::find_if(a_data.geometries.begin(), a_data.geometries.end(), [&](std::pair<RE::BSGeometry*, GeometryData::ObjectInfo>& geometry) {
				return geometry.first == bake.first;
									  });
			if (found == a_data.geometries.end())
			{
				logger::error("{}::{:x} : Geometry {} not found in data", _func_, a_actorID, bake.second.geometryName);
				continue;
			}
			GeometryData::ObjectInfo& objInfo = found->second;
			TextureResourceData newResourceData;
			newResourceData.textureName = bake.second.textureName;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D, detailTexture2D, overlayTexture2D, maskTexture2D;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcShaderResourceView, detailShaderResourceView, overlayShaderResourceView, maskShaderResourceView;
			D3D11_TEXTURE2D_DESC srcDesc = {}, detailDesc = {}, overlayDesc = {}, maskDesc = {}, dstDesc = {}, dstWriteDesc = {};
			D3D11_SHADER_RESOURCE_VIEW_DESC dstShaderResourceViewDesc = {};

			if (!bake.second.srcTexturePath.empty())
			{
				if (!IsDetailNormalMap(bake.second.srcTexturePath))
				{
					D3D11_SHADER_RESOURCE_VIEW_DESC srcShaderResourceViewDesc;
					logger::info("{}::{:x}::{} : {} src texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.srcTexturePath);
					if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.srcTexturePath, srcDesc, srcShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, srcTexture2D))
					{
						dstWriteDesc = srcDesc;
						dstDesc = srcDesc;
						dstShaderResourceViewDesc = srcShaderResourceViewDesc;

						srcShaderResourceViewDesc.Texture2D.MipLevels = 1;
						hr = device->CreateShaderResourceView(srcTexture2D.Get(), &srcShaderResourceViewDesc, srcShaderResourceView.ReleaseAndGetAddressOf());
						if (FAILED(hr))
						{
							logger::error("{}::{:x}::{} : Failed to create src shader resource view ({}|{})", _func_, a_actorID, bake.second.geometryName, hr, bake.second.srcTexturePath);
							srcShaderResourceView = nullptr;
						}
					}
				}
			}
			if (!bake.second.detailTexturePath.empty())
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC detailShaderResourceViewDesc;
				logger::info("{}::{:x}::{} : {} detail texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.detailTexturePath);
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.detailTexturePath, detailDesc, detailShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, detailTexture2D))
				{
					auto tmpDesc = dstDesc;
					dstWriteDesc = detailDesc;
					dstDesc = detailDesc;
					if ((std::uint64_t)detailDesc.Width * (std::uint64_t)detailDesc.Height < (std::uint64_t)tmpDesc.Width * (std::uint64_t)tmpDesc.Height)
					{
						dstWriteDesc.Width = tmpDesc.Width;
						dstWriteDesc.Height = tmpDesc.Height;
						dstDesc.Width = tmpDesc.Width;
						dstDesc.Height = tmpDesc.Height;
					}
					dstShaderResourceViewDesc = detailShaderResourceViewDesc;

					detailShaderResourceViewDesc.Texture2D.MipLevels = 1;
					hr = device->CreateShaderResourceView(detailTexture2D.Get(), &detailShaderResourceViewDesc, detailShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr))
					{
						logger::error("{}::{:x}::{} : Failed to create detail shader resource view ({}|{})", _func_, a_actorID, bake.second.geometryName, hr, bake.second.detailTexturePath);
						detailShaderResourceView = nullptr;
					}
				}
			}
			if (!srcShaderResourceView && !detailShaderResourceView)
			{
				logger::error("{}::{:x}::{} : There is no Normalmap!", _func_, a_actorID, bake.second.geometryName);
				continue;
			}

			if (!bake.second.overlayTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} overlay texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.overlayTexturePath);
				D3D11_SHADER_RESOURCE_VIEW_DESC overlayShaderResourceViewDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.overlayTexturePath, overlayDesc, overlayShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, overlayTexture2D))
				{
					overlayShaderResourceViewDesc.Texture2D.MipLevels = 1;
					hr = device->CreateShaderResourceView(overlayTexture2D.Get(), &overlayShaderResourceViewDesc, overlayShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr))
					{
						overlayShaderResourceView = nullptr;
						logger::error("{}::{:x}::{} : Failed to create overlay shader resource view ({}|{})", _func_, a_actorID, bake.second.geometryName, hr, bake.second.overlayTexturePath);
					}
				}
			}

			if (!bake.second.maskTexturePath.empty())
			{
				logger::info("{}::{:x}::{} : {} mask texture loading...)", _func_, a_actorID, bake.second.geometryName, bake.second.maskTexturePath);
				D3D11_SHADER_RESOURCE_VIEW_DESC maskShaderResourceViewDesc;
				if (Shader::TextureLoadManager::GetSingleton().GetTexture2D(bake.second.maskTexturePath, maskDesc, maskShaderResourceViewDesc, DXGI_FORMAT_UNKNOWN, maskTexture2D))
				{
					maskShaderResourceViewDesc.Texture2D.MipLevels = 1;
					hr = device->CreateShaderResourceView(maskTexture2D.Get(), &maskShaderResourceViewDesc, maskShaderResourceView.ReleaseAndGetAddressOf());
					if (FAILED(hr))
					{
						maskShaderResourceView = nullptr;
						logger::error("{}::{:x}::{} : Failed to create mask shader resource view ({}|{})", _func_, a_actorID, bake.second.geometryName, hr, bake.second.maskTexturePath);
					}
				}
			}

			Microsoft::WRL::ComPtr<ID3D11Texture2D> dstWriteTexture2D;
			dstWriteDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstWriteDesc.Usage = D3D11_USAGE_DEFAULT;
			dstWriteDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			dstWriteDesc.MiscFlags = 0;
			dstWriteDesc.MipLevels = 1;
			dstWriteDesc.CPUAccessFlags = 0;
			hr = device->CreateTexture2D(&dstWriteDesc, nullptr, &dstWriteTexture2D);
			if (FAILED(hr))
			{
				logger::error("{}::{:x}::{} : Failed to create dst texture 2d ({})", _func_, a_actorID, bake.second.geometryName, hr);
				continue;
			}

			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstWriteTextureUAV;
			D3D11_UNORDERED_ACCESS_VIEW_DESC dstUnorderedViewDesc = {};
			dstUnorderedViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstUnorderedViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
			dstUnorderedViewDesc.Texture2D.MipSlice = 0;
			hr = device->CreateUnorderedAccessView(dstWriteTexture2D.Get(), &dstUnorderedViewDesc, &dstWriteTextureUAV);
			if (FAILED(hr))
			{
				logger::error("{}::{:x}::{} : Failed to create dst unordered access view ({})", _func_, a_actorID, bake.second.geometryName, hr);
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
			cbData.hasSrcTexture = srcShaderResourceView ? 1 : 0;
			cbData.hasDetailTexture = detailShaderResourceView ? 1 : 0;
			cbData.hasOverlayTexture = overlayShaderResourceView ? 1 : 0;
			cbData.hasMaskTexture = maskShaderResourceView ? 1 : 0;
			cbData.tangentZCorrection = Config::GetSingleton().GetTangentZCorrection() ? 1 : 0;
			cbData.detailStrength = bake.second.detailStrength;

			D3D11_BUFFER_DESC cbDesc = {};
			cbDesc.ByteWidth = sizeof(ConstBufferData);
			cbDesc.Usage = D3D11_USAGE_DEFAULT;
			cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

			UINT pixelCount = dstDesc.Width * dstDesc.Height;
			D3D11_BUFFER_DESC pixelBufferDesc = {};
			pixelBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
			pixelBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			pixelBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			pixelBufferDesc.CPUAccessFlags = 0;
			pixelBufferDesc.StructureByteStride = 0;
			pixelBufferDesc.ByteWidth = pixelCount * sizeof(std::uint32_t);
			Microsoft::WRL::ComPtr<ID3D11Buffer> pixelBuffer;
			hr = device->CreateBuffer(&pixelBufferDesc, nullptr, &pixelBuffer);
			if (FAILED(hr)) {
				logger::error("{}::{:x}::{} : Failed to create flags buffer ({})", _func_, a_actorID, bake.second.geometryName, hr);
				continue;
			}

			D3D11_UNORDERED_ACCESS_VIEW_DESC pixelBufferUAVDesc = {};
			pixelBufferUAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			pixelBufferUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			pixelBufferUAVDesc.Buffer.FirstElement = 0;
			pixelBufferUAVDesc.Buffer.NumElements = pixelCount;
			pixelBufferUAVDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pixelBufferUAV;
			hr = device->CreateUnorderedAccessView(pixelBuffer.Get(), &pixelBufferUAVDesc, &pixelBufferUAV);
			if (FAILED(hr)) {
				logger::error("{}::{:x}::{} : Failed to create flags buffer unordered access view ({})", _func_, a_actorID, bake.second.geometryName, hr);
				continue;
			}

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
					context->CSGetUnorderedAccessViews(1, 1, &pixelBufferUAV);
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
					context->CSSetUnorderedAccessViews(1, 1, pixelBufferUAV.GetAddressOf(), nullptr);
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
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pixelBufferUAV;
				Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
			private:
			} sb;

			std::mutex constBuffersLock;
			std::uint32_t totalTris = objInfo.indicesCount() / 3;
			std::uint32_t subSize = 1;
			if (Config::GetSingleton().GetDivideTaskQ() == 1)
				subSize = std::max(std::uint32_t(1), totalTris / 15000);
			else if (Config::GetSingleton().GetDivideTaskQ() > 1)
				subSize = std::max(std::uint32_t(1), totalTris / 10000);

			const std::uint32_t numSubTris = (totalTris + subSize - 1) / subSize;
			const std::uint32_t dispatch = (numSubTris + 64 - 1) / 64;
			std::vector<std::future<void>> gpuTasks;
			for (std::size_t subIndex = 0; subIndex < subSize; subIndex++)
			{
				const std::uint32_t trisStart = objInfo.indicesStart + subIndex * numSubTris * 3;
				gpuTasks.push_back(gpuTask->submitAsync([&, trisStart, subIndex]() {
#ifdef BAKE_TEST2
					GPUPerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + bake.second.geometryName + "::" + std::to_string(subIndex), false, false);
#endif // BAKE_TEST2
					auto cbData_ = cbData;
					cbData_.indicesStart = trisStart;
					D3D11_SUBRESOURCE_DATA cbInitData = {};
					cbInitData.pSysMem = &cbData_;
					Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
					hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
					if (FAILED(hr)) {
						logger::error("{}::{:x}::{} : Failed to create const buffer ({})", _func_, a_actorID, bake.second.geometryName, hr);
						return;
					}
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					sb.Backup(context);
					context->CSSetShader(shader.Get(), nullptr, 0);
					context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
					context->CSSetShaderResources(0, 1, GeometryResourceDataMap[a_actorID].vertexSRV.GetAddressOf());
					context->CSSetShaderResources(1, 1, GeometryResourceDataMap[a_actorID].uvSRV.GetAddressOf());
					context->CSSetShaderResources(2, 1, GeometryResourceDataMap[a_actorID].normalSRV.GetAddressOf());
					context->CSSetShaderResources(3, 1, GeometryResourceDataMap[a_actorID].tangentSRV.GetAddressOf());
					context->CSSetShaderResources(4, 1, GeometryResourceDataMap[a_actorID].bitangentSRV.GetAddressOf());
					context->CSSetShaderResources(5, 1, GeometryResourceDataMap[a_actorID].indicesSRV.GetAddressOf());
					context->CSSetShaderResources(6, 1, srcShaderResourceView.GetAddressOf());
					context->CSSetShaderResources(7, 1, detailShaderResourceView.GetAddressOf());
					context->CSSetShaderResources(8, 1, overlayShaderResourceView.GetAddressOf());
					context->CSSetShaderResources(9, 1, maskShaderResourceView.GetAddressOf());
					context->CSSetUnorderedAccessViews(0, 1, dstWriteTextureUAV.GetAddressOf(), nullptr);
					context->CSSetUnorderedAccessViews(1, 1, pixelBufferUAV.GetAddressOf(), nullptr);
					context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
					context->Dispatch(dispatch, 1, 1);
					sb.Revert(context);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
					constBuffersLock.lock();
					newResourceData.constBuffers.push_back(constBuffer);
					constBuffersLock.unlock();
#ifdef BAKE_TEST2
					GPUPerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID) + "::" + bake.second.geometryName + "::" + std::to_string(subIndex), true, false);
#endif // BAKE_TEST2
				}));
			}
			for (auto& task : gpuTasks) {
				task.get();
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
				logger::error("{}::{:x} : Failed to create dst texture ({})", _func_, a_actorID, hr);
				continue;
			}

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->CopySubresourceRegion(
				dstTexture2D.Get(),
				0,
				0, 0, 0,
				dstWriteTexture2D.Get(),
				0,
				nullptr);
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			dstShaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			dstShaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
			dstShaderResourceViewDesc.Texture2D.MipLevels = -1;
			hr = device->CreateShaderResourceView(dstTexture2D.Get(), &dstShaderResourceViewDesc, &dstShaderResourceView);
			if (FAILED(hr)) {
				logger::error("{}::{:x} : Failed to create ShaderResourceView ({})", _func_, a_actorID, hr);
				continue;
			}

			//TexturePostProcessingGPU(newResourceData, 0.3f, 1.0f, dstShaderResourceView, dstTexture2D);
			BleedTextureGPU(newResourceData, margin, dstShaderResourceView, dstTexture2D);

			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			context->GenerateMips(dstShaderResourceView.Get());
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

			newResourceData.GetQuery(device, context);

			RE::NiPointer<RE::NiSourceTexture> output = nullptr;
			bool texCreated = false;
			Shader::TextureLoadManager::GetSingleton().CreateNiTexture(bake.second.textureName, bake.second.detailTexturePath.empty() ? "None" : bake.second.detailTexturePath, dstTexture2D, dstShaderResourceView, output, texCreated);
			if (!output)
				continue;

			NormalMapResult newNormalMapResult;
			newNormalMapResult.geometry = bake.first;
			newNormalMapResult.vertexCount = objInfo.vertexCount();
			newNormalMapResult.geoName = bake.second.geometryName;
			newNormalMapResult.textureName = bake.second.textureName;
			newNormalMapResult.normalmap = output;
			result.push_back(newNormalMapResult);

			newResourceData.srcTexture2D = srcTexture2D;
			newResourceData.srcShaderResourceView = srcShaderResourceView;
			newResourceData.detailTexture2D = detailTexture2D;
			newResourceData.detailShaderResourceView = detailShaderResourceView;
			newResourceData.overlayTexture2D = overlayTexture2D;
			newResourceData.overlayShaderResourceView = overlayShaderResourceView;
			newResourceData.maskTexture2D = maskTexture2D;
			newResourceData.maskShaderResourceView = maskShaderResourceView;
			newResourceData.dstWriteTexture2D = dstWriteTexture2D;
			newResourceData.dstWriteTextureUAV = dstWriteTextureUAV;
			newResourceData.pixelBuffer = pixelBuffer;
			newResourceData.pixelBufferUAV = pixelBufferUAV;

			ResourceDataMapLock.lock_shared();
			ResourceDataMap.push_back(newResourceData);
			ResourceDataMapLock.unlock_shared();
			logger::info("{}::{:x}::{} : normalmap baked", _func_, a_actorID, bake.second.geometryName);
		}
		GeometryResourceDataMap[a_actorID].GetQuery(device, context);
#ifdef BAKE_TEST1
		PerformanceLog(std::string(_func_) + "::" + GetHexStr(a_actorID), true, false);
#endif // BAKE_TEST1
		return result;
	}

	bool ObjectNormalMapUpdater::IsDetailNormalMap(std::string a_normalMapPath)
	{
		constexpr std::string_view prefix = "Textures\\";
		constexpr std::string_view n_suffix = "_n";
		if (a_normalMapPath.empty())
			return false;
		if (!stringStartsWith(a_normalMapPath, prefix.data()))
			a_normalMapPath = prefix.data() + a_normalMapPath;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		return stringEndsWith(filename, n_suffix.data());
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
	bool ObjectNormalMapUpdater::BleedTexture(std::uint8_t* pData, UINT width, UINT height, UINT RowPitch, std::uint32_t margin)
	{
#ifdef BLEED_TEST1
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), false, false);
#endif // BLEED_TEST1

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

		std::vector<std::future<void>> processes;
		concurrency::concurrent_unordered_map<std::uint32_t*, RGBA> resultColorMap;
		for (UINT y = 0; y < height; y++) {
			std::uint8_t* rowData = pData + y * RowPitch;
			for (int x = 0; x < width; ++x)
			{
				processes.push_back(processingThreads->submitAsync([&, rowData, y, x]() {
					std::uint32_t* pixel = reinterpret_cast<uint32_t*>(rowData + x * 4);
					if (IsValidPixel(*pixel))
						return;

					RGBA averageColor(0.0f, 0.0f, 0.0f, 0.0f);
					std::uint8_t validCount = 0;
#pragma unroll(8)
					for (std::uint8_t i = 0; i < 8; i++)
					{
						DirectX::XMINT2 nearCoord = { (INT)x + offsets[i].x, (INT)y + offsets[i].y };
						if (nearCoord.x < 0 || nearCoord.y < 0 ||
							nearCoord.x >= width || nearCoord.y >= height)
							return;

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
						return;

					RGBA resultColor = averageColor / validCount;
					resultColorMap[pixel] = resultColor;
				}));
			}
		}
		for (auto& process : processes)
		{
			process.get();
		}
		for (auto& map : resultColorMap)
		{
			*map.first = map.second.GetReverse();
		}
#ifdef BLEED_TEST1
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), true, false);
#endif // BLEED_TEST1
		return true;
	}

	bool ObjectNormalMapUpdater::BleedTextureGPU(TextureResourceData& resourceData, std::uint32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		std::string_view _func_ = __func__;
#ifdef BLEED_TEST1
		PerformanceLog(std::string(_func_) + "::" + textureName, false, false);
#endif // BLEED_TEST1

		if (margin <= 0)
			return true;

		logger::debug("{} : Bleed texture... {}", _func_, margin);

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
		desc.MipLevels = srvDesc.Texture2D.MipLevels;
		desc.CPUAccessFlags = 0;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
		hr = device->CreateTexture2D(&desc, nullptr, &texture2D);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create const buffer ({})", _func_, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		hr = device->CreateUnorderedAccessView(texture2D.Get(), &uavDesc, &uav);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}

#ifdef BLEED_TEST1
		PerformanceLog(std::string(_func_) + "::" + textureName, true, false);
#endif // BLEED_TEST1

		Shader::ShaderManager::GetSingleton().ShaderContextLock();
		context->CopyResource(texture2D.Get(), texInOut.Get());
		Shader::ShaderManager::GetSingleton().ShaderContextUnlock();

		std::mutex constBuffersLock;
		std::vector<std::future<void>> gpuTasks;
		const std::uint32_t subResolution = 4096 / (1u << Config::GetSingleton().GetDivideTaskQ());
		const DirectX::XMUINT2 dispatch = { (std::min(width, subResolution) + 8 - 1) / 8, (std::min(height, subResolution) + 8 - 1) / 8};
		const std::uint32_t marginUnit = std::max(std::uint32_t(1), std::min(margin, subResolution / width + subResolution / height));
		const std::uint32_t marginMax = std::max(std::uint32_t(1), margin / marginUnit);
		const std::uint32_t subXSize = std::max(std::uint32_t(1), width / subResolution);
		const std::uint32_t subYSize = std::max(std::uint32_t(1), height / subResolution);
		for (std::uint32_t mi = 0; mi < marginMax; mi++)
		{
			for (std::uint32_t subY = 0; subY < subYSize; subY++)
			{
				for (std::uint32_t subX = 0; subX < subXSize; subX++)
				{
					gpuTasks.push_back(gpuTask->submitAsync([&, subX, subY, mi]() {
#ifdef BLEED_TEST2
						GPUPerformanceLog(std::string(_func_) + "::" + resourceData.textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
										  + "::" + std::to_string(subX) + "|" + std::to_string(subY) + "::" + std::to_string(mi), false, false);
#endif // BLEED_TEST2
						auto cbData_ = cbData;
						cbData_.widthStart = subResolution * subX;
						cbData_.heightStart = subResolution * subY;
						D3D11_SUBRESOURCE_DATA cbInitData = {};
						cbInitData.pSysMem = &cbData_;
						Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
						hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
						if (FAILED(hr)) {
							logger::error("{} : Failed to create const buffer ({})", _func_, hr);
							return;
						}
						Shader::ShaderManager::GetSingleton().ShaderContextLock();
						sb.Backup(context);
						context->CSSetShader(shader.Get(), nullptr, 0);
						context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
						context->CSSetShaderResources(0, 1, srvInOut.GetAddressOf());
						context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
						context->Dispatch(dispatch.x, dispatch.y, 1);
						if (subX == subXSize - 1 && subY == subYSize - 1)
							context->CopyResource(texInOut.Get(), texture2D.Get());
						for (std::uint32_t mu = 1; mu < marginUnit; mu++)
						{
							context->Dispatch(dispatch.x, dispatch.y, 1);
							context->CopyResource(texInOut.Get(), texture2D.Get());
						}
						sb.Revert(context);
						Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
						constBuffersLock.lock();
						resourceData.bleedTextureData.constBuffers.push_back(constBuffer);
						constBuffersLock.unlock();
#ifdef BLEED_TEST2
						GPUPerformanceLog(std::string(_func_) + "::" + resourceData.textureName + "::" + std::to_string(width) + "|" + std::to_string(height)
										  + "::" + std::to_string(subX) + "|" + std::to_string(subY) + "::" + std::to_string(mi), true, false);
#endif // BLEED_TEST2
					}));
				}
			}
		}
		for (auto& task : gpuTasks)
		{
			task.get();
		}

		resourceData.bleedTextureData.texture2D = texture2D;
		resourceData.bleedTextureData.uav = uav;
		return true;
	}

	bool ObjectNormalMapUpdater::TexturePostProcessingGPU(TextureResourceData& resourceData, float threshold, float blendStrength, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
	{
		std::string_view _func_ = __func__;
#ifdef BLEED_TEST1
		PerformanceLog(std::string(__func__) + "::" + std::to_string(width) + "|" + std::to_string(height), false, false);
#endif // BLEED_TEST1
		if (threshold <= floatPrecision || blendStrength <= floatPrecision)
			return true;

		logger::debug("{}::{} : TexturePostProcessing... {} / {}", _func_, resourceData.textureName, threshold, blendStrength);

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

			float threshold;
			float blendStrength;
			UINT padding1;
			UINT padding2;
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
		cbData.threshold = threshold;
		cbData.blendStrength = blendStrength;

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
			logger::error("{} : Failed to create const buffer ({})", _func_, hr);
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
			logger::error("{} : Failed to create const buffer ({})", _func_, hr);
			return false;
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
		hr = device->CreateUnorderedAccessView(texture2D.Get(), &uavDesc, &uav);
		if (FAILED(hr)) {
			logger::error("{} : Failed to create unordered access view ({})", _func_, hr);
			return false;
		}
#ifdef BLEED_TEST1
		PerformanceLog(std::string(_func_) + "::" + textureName, true, false);
#endif // BLEED_TEST1

		const DirectX::XMUINT2 dispatch = { (width + 8 - 1) / 8, 1 };
		gpuTask->submitAsync([&]() {
#ifdef BLEED_TEST2
			GPUPerformanceLog(std::string(_func_) + "::" + textureName + "::" + std::to_string(width) + "|" + std::to_string(height), false, false);
#endif // BLEED_TEST2
			Shader::ShaderManager::GetSingleton().ShaderContextLock();
			sb.Backup(context);
			context->CopyResource(texture2D.Get(), texInOut.Get());
			context->CSSetShader(shader.Get(), nullptr, 0);
			context->CSSetShaderResources(0, 1, srvInOut.GetAddressOf());
			context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
			for (std::uint32_t i = 0; i < height; i++)
			{
				auto cbData_ = cbData;
				cbData_.widthStart = 0;
				cbData_.heightStart = i;
				D3D11_SUBRESOURCE_DATA cbInitData = {};
				cbInitData.pSysMem = &cbData_;
				Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
				hr = device->CreateBuffer(&cbDesc, &cbInitData, &constBuffer);
				if (SUCCEEDED(hr)) {
					context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
					context->Dispatch(dispatch.x, dispatch.y, 1);
					resourceData.texturePostProcessingData.constBuffer.push_back(constBuffer);
				}
			}
			sb.Revert(context);
			context->CopyResource(texInOut.Get(), texture2D.Get());
			Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
#ifdef BLEED_TEST2
			GPUPerformanceLog(std::string(_func_) + "::" + textureName + "::" + std::to_string(width) + "|" + std::to_string(height), true, false);
#endif // BLEED_TEST2
		}).get();
		resourceData.texturePostProcessingData.texture2D = texture2D;
		resourceData.texturePostProcessingData.uav = uav;

		return true;
	}

	void ObjectNormalMapUpdater::GPUPerformanceLog(std::string funcStr, bool isEnd, bool isAverage, std::uint32_t args)
	{
		struct GPUTimer
		{
			Microsoft::WRL::ComPtr<ID3D11Query> startQuery;
			Microsoft::WRL::ComPtr<ID3D11Query> endQuery;
			Microsoft::WRL::ComPtr<ID3D11Query> disjointQuery;
		};

		if (!PerformanceCheck)
			return;

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
								 (double)average / tick * 100
					);
					if (PerformanceCheckConsolePrint) {
						auto Console = RE::ConsoleLog::GetSingleton();
						if (Console)
							Console->Print("%s average time: %lldms%s=> %.6f%%", funcStr.c_str(), average,
										   funcAverageArgs[funcStr] > 0 ? (std::string(" with average count ") + std::to_string(funcAverageArgs[funcStr] / funcAverageCount[funcStr]) + " ").c_str() : " ",
										   (double)average / tick * 100
							);
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
									   (double)duration_ms / tick * 100
						);
				}
			}
		}
	}

	void ObjectNormalMapUpdater::WaitForGPU()
	{
		return;

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
		return;
	}
}
