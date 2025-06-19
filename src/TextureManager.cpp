#include "TextureManager.h"

namespace Mus {
	void TextureManager::onEvent(const FrameEvent& e)
	{
		RunDelayTask();
	}
	void TextureManager::onEvent(const FacegenNiNodeEvent& e) 
	{

	}
	void TextureManager::onEvent(const ActorChangeHeadPartEvent& e) 
	{

	}
	void TextureManager::onEvent(const ArmorAttachEvent& e) 
	{
		if (!e.actor)
			return;
		if (!isPlayer(e.actor->formID))
			return;
		if (!e.hasAttached || !e.attachedNode)
			return;

		RE::FormID id = e.actor->formID;
		std::uint32_t bipedSlot = e.bipedSlot;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, bipedSlot, delayTaskID]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				logger::error("{:x} {} : invalid reference. so skip", id, magic_enum::enum_name(RE::BIPED_OBJECT(bipedSlot)).data());
				return;
			}
			std::vector<RE::BSGeometry*> geometries = GetGeometries(actor, bipedSlot);
			QBakeObjectNormalMap(actor, geometries, bipedSlot, true);
		});
	}

	void TextureManager::RunDelayTask()
	{
		if (delayTask.empty())
			return;
		
		delayTaskLock.lock();
		std::unordered_map<std::string, std::function<void()>> delayTask_ = delayTask;
		delayTask.clear();
		delayTaskLock.unlock();
		for (auto& task : delayTask_)
		{
			task.second();
		}
	}

	std::vector<RE::BSGeometry*> Mus::TextureManager::GetGeometries(RE::NiAVObject* a_root, std::string a_geoName)
	{
		std::vector<RE::BSGeometry*> geometries;
		if (!a_root)
			return geometries;
		if (a_geoName.empty())
		{
			RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
				if (!geo || geo->name.empty())
					return RE::BSVisit::BSVisitControl::kContinue;
				geometries.push_back(geo);
				return RE::BSVisit::BSVisitControl::kContinue;
			});
		}
		else
		{
			RE::NiAVObject* obj = a_root->GetObjectByName(a_geoName.c_str());
			if (RE::BSGeometry* geo = obj ? obj->AsGeometry() : nullptr; geo)
				geometries.push_back(geo);
		}
		return geometries;
	}
	std::vector<RE::BSGeometry*> TextureManager::GetGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot)
	{
		std::vector<RE::BSGeometry*> geometries;
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return geometries;
		auto armo = a_actor->GetSkin(RE::BIPED_MODEL::BipedObjectSlot(1 << bipedSlot));
		if (!armo)
			return geometries;
		
		for (auto& arma : armo->armorAddons)
		{
			if (!arma)
				continue;
			char addonString[MAX_PATH]{ '\0' };
			arma->GetNodeName(addonString, a_actor, armo, -1);
			auto root = a_actor->loadedData->data3D->GetObjectByName(addonString);
			if (!root)
				continue;
			RE::BSVisit::TraverseScenegraphGeometries(root, [&geometries](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
				if (!geometry || geometry->name.empty())
					return RE::BSVisit::BSVisitControl::kContinue;
				geometries.push_back(geometry);
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			if (!geometries.empty())
				continue;
		}
		return geometries;
	}
	std::vector<RE::BSGeometry*> TextureManager::GetGeometries(std::string a_fileName)
	{
		std::vector<RE::BSGeometry*> geometries;

		if (a_fileName.empty() || stringEndsWith(a_fileName, ".nif"))
			return geometries;
		if (stringStartsWith(a_fileName, "Data\\"))
			a_fileName = stringRemoveStarts(a_fileName, "Data\\");
		if (!stringStartsWith(a_fileName, "meshes\\"))
			a_fileName = "meshes\\" + a_fileName;

		RE::BSGeometry* geo = nullptr;

		std::uint8_t niStreamMemory[sizeof(RE::NiStream)];
		RE::NiStream* niStream = (RE::NiStream*)niStreamMemory;
		NiStreamCtor(niStream);

		RE::BSResourceNiBinaryStream binaryStream(a_fileName.c_str());
		if (!binaryStream.good()) {
			logger::critical("{} : Couldn't read nif file!", a_fileName);
			NiStreamDtor(niStream);
			return geometries;
		}

		if (!niStream->Load1(&binaryStream))
		{
			if (!niStream->Load3((GetRuntimeDataDirectory() + a_fileName).c_str()))
			{
				logger::critical("{} : Couldn't read nif file!", a_fileName);
				NiStreamDtor(niStream);
				return geometries;
			}
		}

		if (niStream->topObjects.size() > 0)
		{
			RE::NiNode* root = niStream->topObjects[0]->AsNode();
			if (root)
			{
				for (auto& child : root->GetChildren())
				{
					if (!child || child->name.empty())
						continue;
					geo = child->AsGeometry();
					if (!geo)
						continue;
					geometries.push_back(geo);
				}
			}
		}
		NiStreamDtor(niStream);
		return geometries;
	}

	bool TextureManager::QBakeObjectNormalMap(RE::Actor* a_actor, std::vector<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot, bool rebake)
	{
		std::string bipedSlotName = magic_enum::enum_name(RE::BIPED_OBJECT(bipedSlot)).data();
		if (!a_actor || a_srcGeometies.empty() || bipedSlot >= RE::BIPED_OBJECT::kEditorTotal)
		{
			logger::error("{} {} : Invalid parameters", __func__, bipedSlotName);
			return false;
		}
		RE::TESObjectARMO* armor = a_actor->GetSkin(RE::BGSBipedObjectForm::BipedObjectSlot(1 << bipedSlot));
		if (!armor)
		{
			logger::error("{} {} : Invalid armor", __func__, bipedSlotName);
			return false;
		}

		struct BakeData {
			std::string geometryName;
			std::string textureName;
			GeometryData data;
			std::string srcTexturePath;
			std::string maskTexturePath = "Textures\\TextureManager\\BakeObjectNormalMapMask.dds";
			float smooth = 60.0f;
		};
		std::vector<BakeData> bakeDataList;

		for (auto& geo : a_srcGeometies)
		{
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (!geo || geo->name.empty())
				continue;
			if (!geo->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect])
				continue;
			auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
			if (!effect)
				continue;
			auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
			if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
				continue;
			RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (!material || !material->normalTexture || !material->textureSet)
				continue;
			std::string texturePath = GetTexturePath(material->textureSet.get(), RE::BSTextureSet::Texture::kNormal);
			if (texturePath.empty())
				continue;

			std::string textureName = GetTextureName(a_actor, armor, bipedSlot, geo);
			if (!rebake)
			{
				auto result = Shader::TextureLoadManager::GetSingleton().GetOrgTexturePath(textureName);
				if (!result.empty())
				{
					RE::NiPointer<RE::NiSourceTexture> normalTexture;
					if (Shader::TextureLoadManager::CreateSourceTexture(textureName, normalTexture) == 0)
					{
						material->normalTexture = normalTexture;
						logger::info("{:x} {} {} : {} found the already baked normalmap. so skip", a_actor->formID, a_actor->GetName(), bipedSlotName, geo->name.c_str());
						continue;
					}
				}
			}
			BakeData bakeData;
			bakeData.geometryName = geo->name.c_str();
			bakeData.textureName = textureName;
			bakeData.data = GetGeometryData(geo);
			bakeData.srcTexturePath = texturePath;
			bakeDataList.push_back(bakeData);
			logger::info("{:x} {} {} : {} (vertices {} / uvs {} / tris {}) queue added on bake object normalmap", a_actor->formID, a_actor->GetName(), bipedSlotName,
				geo->name.c_str(), bakeData.data.vertices.size(), bakeData.data.uvs.size(), bakeData.data.indices.size() / 3);
		}

		RE::FormID id = a_actor->formID;
		auto taskID = AttachBakeObjectNormalMapTaskID(id, bipedSlot);
		std::thread bakeThread([this, id, bakeDataList, bipedSlotName, bipedSlot, taskID]() {
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			std::this_thread::yield();
			struct textureResult {
				std::string geometryName;
				RE::NiPointer<RE::NiSourceTexture> texture;
			};
			std::vector<textureResult> results;
			for (auto& bakeData : bakeDataList)
			{
				if (GetBakeObjectNormalMapTaskID(id, bipedSlot) != taskID)
				{
					logger::info("{:x} {} : {} cancel queue for bake object normalmap", id, bakeData.geometryName, bipedSlotName);
					return;
				}
				auto texture = BakeObjectNormalMap(bakeData.textureName, bakeData.data, bakeData.srcTexturePath, bakeData.maskTexturePath, bakeData.smooth);
				if (!texture)
				{
					logger::error("{:x} {} : {} Failed to bake object normalmap", id, bakeData.geometryName, bipedSlotName);
					continue;
				}
				textureResult result;
				result.geometryName = bakeData.geometryName;
				result.texture = texture;
				results.push_back(result);
			}
			RegisterDelayTask(GetDelayTaskID(id, bipedSlot), [this, id, results, bipedSlotName, bipedSlot, taskID]() {
				auto refr = GetFormByID<RE::TESObjectREFR*>(id);
				if (!refr || !refr->loadedData || !refr->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so cancel all current queue for bake object normalmap", id, bipedSlotName);
					DetachBakeObjectNormalMapTaskID(id, bipedSlot, taskID);
					return;
				}
				auto root = refr->loadedData->data3D.get();
				for (auto& result : results)
				{
					using State = RE::BSGeometry::States;
					using Feature = RE::BSShaderMaterial::Feature;
					if (GetBakeObjectNormalMapTaskID(id, bipedSlot) != taskID)
					{
						logger::error("{:x} {} {} : invalid task queue. so cancel all current queue for bake object normalmap", id, refr->GetName(), bipedSlotName);
						return;
					}
					auto obj = root->GetObjectByName(result.geometryName.c_str());
					auto geo = obj ? obj->AsGeometry() : nullptr;
					if (!geo)
					{
						logger::error("{:x} {} {} : {} invalid geometry. so cancel bake object normalmap for this geoemtry", id, refr->GetName(), result.geometryName.c_str(), bipedSlotName);
						continue;
					}
					auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
					if (!effect)
						continue;
					auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
					if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
						continue;
					RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
					if (!material || !material->normalTexture)
						continue;
					material->normalTexture = result.texture;
					logger::info("{:x} {} {} : {} bake object normalmap done", id, refr->GetName(), geo->name.c_str(), bipedSlotName);
				}
				DetachBakeObjectNormalMapTaskID(id, bipedSlot, taskID);
			});
		});
		bakeThread.detach();

		return true;
	}
	RE::NiPointer<RE::NiSourceTexture> TextureManager::BakeObjectNormalMap(std::string textureName, GeometryData a_data, std::string a_srcTexturePath, std::string a_maskTexturePath, float a_normalSmooth, std::uint32_t a_subdivision, std::uint32_t a_vertexSmooth, float a_vertexSmoothStrength)
	{
		PerformanceLog(std::string(__func__) + "::" + textureName, false, false);

		if (a_data.vertices.empty() || a_data.indices.empty()
			|| a_data.vertices.size() != a_data.uvs.size()
			|| textureName.empty())
		{
			logger::error("{} : Invalid parameters", __func__);
			return nullptr;
		}

		Subdivision(a_data, a_subdivision);
		VertexSmooth(a_data, a_vertexSmoothStrength, a_vertexSmooth);
		RecalculateNormals(a_data, a_normalSmooth);

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
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
				std::this_thread::yield();
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

							DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(
								DirectX::XMVectorAdd(
									DirectX::XMVectorAdd(
										DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n0), bary.x),
										DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n1), bary.y)),
									DirectX::XMVectorScale(DirectX::XMLoadFloat3(&n2), bary.z))
							);

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

	RE::NiPointer<RE::NiSkinPartition> TextureManager::GetSkinPartition(RE::BSGeometry* a_geo)
	{
		if (!a_geo)
			return nullptr;
		if (!a_geo->GetGeometryRuntimeData().skinInstance)
			return nullptr;
		RE::NiSkinInstance* skinInstance = a_geo->GetGeometryRuntimeData().skinInstance.get();
		if (!skinInstance->skinPartition)
			return nullptr;
		return skinInstance->skinPartition;
	}
	TextureManager::GeometryData TextureManager::GetGeometryData(RE::BSGeometry* a_geo)
	{
		GeometryData data;

		if (!a_geo || a_geo->name.empty())
			return data;

		RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape();
		RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return data;

		std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		data.desc = a_geo->GetGeometryRuntimeData().vertexDesc;

		data.hasVertices = data.desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX);
		data.hasUV = data.desc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
		data.hasNormals = false; //data.desc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
		data.hasTangents = false; //data.desc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT);

		RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geo);
		if (!skinPartition)
			return data;

		logger::info("{} {} : get geometry data...", __func__, a_geo->name.c_str());

		vertexCount = vertexCount ? vertexCount : skinPartition->vertexCount;
		data.vertices.resize(vertexCount);
		data.uvs.resize(vertexCount);
		data.normals.resize(vertexCount);
		//data.bitangent.resize(vertexCount);
		//data.tangent.resize(vertexCount);

		std::uint32_t vertexSize = data.desc.GetSize();
		std::uint8_t* vertexBlock = skinPartition->partitions[0].buffData->rawVertexData;

		if (dynamicTriShape)
		{
			DirectX::XMVECTOR* vertex = reinterpret_cast<DirectX::XMVECTOR*>(dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData);
			for (std::uint32_t i = 0; i < vertexCount; i++)
			{
				DirectX::XMStoreFloat3(&data.vertices[i], vertex[i]);
			}
		}

		for (std::uint32_t i = 0; i < vertexCount; i++)
		{
			std::uint8_t* block = &vertexBlock[i * vertexSize];
			if (data.hasVertices)
			{
				data.vertices[i] = *reinterpret_cast<DirectX::XMFLOAT3*>(block);
				block += 12;

				//data.bitangent[i].x = *reinterpret_cast<float*>(block);
				block += 4;
			}

			if (data.hasUV)
			{
				data.uvs[i].x = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block));
				data.uvs[i].y = DirectX::PackedVector::XMConvertHalfToFloat(*reinterpret_cast<std::uint16_t*>(block + 2));
				block += 4;
			}

			/*if (data.hasNormals)
			{
				data.normal[i].x = static_cast<float>(*block) / 255.0f;
				data.normal[i].y = static_cast<float>(*(block + 1)) / 255.0f;
				data.normal[i].z = static_cast<float>(*(block + 2)) / 255.0f;
				block += 3;

				data.bitangent[i].y = static_cast<float>(*block);
				block += 1;

				if (data.hasTangents)
				{
					data.tangent[i].x = static_cast<float>(*block) / 255.0f;
					data.tangent[i].y = static_cast<float>(*(block + 1)) / 255.0f;
					data.tangent[i].z = static_cast<float>(*(block + 2)) / 255.0f;
					block += 3;

					data.bitangent[i].z = static_cast<float>(*block);
				}
			}*/
		}

		for (std::uint32_t p = 0; p < skinPartition->numPartitions; p++)
		{
			for (std::uint32_t t = 0; t < skinPartition->partitions[p].triangles * 3; t++)
			{
				data.indices.push_back(skinPartition->partitions[p].triList[t]);
			}
		}

		logger::info("{} {} : get geometry data done => vertices {} / uvs {} / normals {} / tangents {} / tris {}", __func__, a_geo->name.c_str(), 
					 data.vertices.size(), data.uvs.size(), data.normals.size(), data.tangents.size(), data.indices.size() / 3);
		return data;
	}

	void TextureManager::RecalculateNormals(GeometryData& a_data, float a_smooth)
	{
		if (a_data.vertices.empty() || a_data.normals.empty() || a_data.vertices.size() != a_data.normals.size() || a_data.indices.empty())
			return;

		logger::info("{} : normals {} re-calculate...", __func__, a_data.normals.size());
		const float smoothCos = cosf(DirectX::XMConvertToRadians(a_smooth));

		struct Vec3Hash {
			size_t operator()(const DirectX::XMFLOAT3& v) const {
				size_t hx = std::hash<int>()(int(v.x * 10000));
				size_t hy = std::hash<int>()(int(v.y * 10000));
				size_t hz = std::hash<int>()(int(v.z * 10000));
				return ((hx ^ (hy << 1)) >> 1) ^ (hz << 1);
			}
		};

		struct Vec3Equal {
			bool operator()(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const {
				const float eps = 1e-4f;
				return fabs(a.x - b.x) < eps && fabs(a.y - b.y) < eps && fabs(a.z - b.z) < eps;
			}
		};

		struct FaceNormal {
			size_t v0, v1, v2;
			DirectX::XMVECTOR normal;
		};

		std::unordered_map<DirectX::XMFLOAT3, std::vector<size_t>, Vec3Hash, Vec3Equal> positionMap;
		std::vector<FaceNormal> faceNormals;
		for (std::size_t i = 0; i < a_data.indices.size(); i += 3)
		{
			std::uint32_t v0 = a_data.indices[i + 0];
			std::uint32_t v1 = a_data.indices[i + 1];
			std::uint32_t v2 = a_data.indices[i + 2];

			DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&a_data.vertices[v0]);
			DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&a_data.vertices[v1]);
			DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&a_data.vertices[v2]);

			DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p2, p0)));
			faceNormals.push_back({ v0, v1, v2, normal });

			positionMap[a_data.vertices[v0]].push_back(faceNormals.size() - 1);
			positionMap[a_data.vertices[v1]].push_back(faceNormals.size() - 1);
			positionMap[a_data.vertices[v2]].push_back(faceNormals.size() - 1);
		}

		for (std::uint32_t i = 0; i < a_data.vertices.size(); ++i)
		{
			const DirectX::XMFLOAT3& pos = a_data.vertices[i];

			DirectX::XMVECTOR nSum = DirectX::XMVectorZero();
			DirectX::XMVECTOR nSelf = DirectX::XMVectorZero();

			std::vector<size_t> faceIndices = positionMap[pos];
			for (size_t fi : faceIndices) {
				const FaceNormal& f = faceNormals[fi];
				if (f.v0 == i || f.v1 == i || f.v2 == i) {
					nSelf = f.normal;
					break;
				}
			}

			for (size_t fi : faceIndices) {
				const DirectX::XMVECTOR fn = faceNormals[fi].normal;
				float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(fn, nSelf));
				if (dot >= smoothCos) {
					nSum = DirectX::XMVectorAdd(nSum, fn);
				}
			}

			DirectX::XMStoreFloat3(&a_data.normals[i], DirectX::XMVector3Normalize(nSum));
		}
		logger::info("{} : normals {} re-calculated", __func__, a_data.normals.size());
		return;
	}
	void TextureManager::Subdivision(GeometryData& a_data, std::uint32_t a_subCount)
	{
		if (a_subCount == 0)
			return;

		logger::info("{} : {} subdivition({})...", __func__, a_data.vertices.size(), a_subCount);
		GeometryData subdividedData;
		subdividedData.vertices = a_data.vertices;
		subdividedData.uvs = a_data.uvs;
		subdividedData.normals = a_data.normals;
		subdividedData.tangents = a_data.tangents;
		subdividedData.bitangent = a_data.bitangent;

		std::map<std::pair<uint32_t, uint32_t>, uint32_t> midpointMap;
		auto getMidpointIndex = [&](uint32_t i0, uint32_t i1) -> uint32_t {
			auto key = std::minmax(i0, i1);
			if (auto it = midpointMap.find(key); it != midpointMap.end())
				return it->second;

			uint32_t index = subdividedData.vertices.size();

			const auto& v0 = subdividedData.vertices[i0];
			const auto& v1 = subdividedData.vertices[i1];
			DirectX::XMFLOAT3 midVertex;
			midVertex = {
				(v0.x + v1.x) * 0.5f,
				(v0.y + v1.y) * 0.5f,
				(v0.z + v1.z) * 0.5f
			};
			subdividedData.vertices.push_back(midVertex);

			if (a_data.hasUV)
			{
				const auto& u0 = subdividedData.uvs[i0];
				const auto& u1 = subdividedData.uvs[i1];
				DirectX::XMFLOAT2 midUV;
				midUV = {
					(u0.x + u1.x) * 0.5f,
					(u0.y + u1.y) * 0.5f
				};
				subdividedData.uvs.push_back(midUV);
			}

			if (a_data.hasNormals)
			{
				const auto& n0 = subdividedData.normals[i0];
				const auto& n1 = subdividedData.normals[i1];
				DirectX::XMFLOAT3 midNormal;
				midNormal = {
					(n0.x + n1.x) * 0.5f,
					(n0.y + n1.y) * 0.5f,
					(n0.z + n1.z) * 0.5f
				};
				subdividedData.normals.push_back(midNormal);
			}

			if (a_data.hasTangents)
			{
				const auto& b0 = subdividedData.bitangent[i0];
				const auto& b1 = subdividedData.bitangent[i1];
				DirectX::XMFLOAT3 midBitTangent;
				midBitTangent = {
					(b0.x + b1.x) * 0.5f,
					(b0.y + b1.y) * 0.5f,
					(b0.z + b1.z) * 0.5f
				};
				subdividedData.bitangent.push_back(midBitTangent);

				const auto& t0 = subdividedData.tangents[i0];
				const auto& t1 = subdividedData.tangents[i1];
				DirectX::XMFLOAT3 midTangent;
				midTangent = {
					(t0.x + t1.x) * 0.5f,
					(t0.y + t1.y) * 0.5f,
					(t0.z + t1.z) * 0.5f
				};
				subdividedData.tangents.push_back(midTangent);
			}
			midpointMap[key] = index;
			return index;
		};

		for (std::uint32_t i = 0; i < a_data.indices.size(); i += 3)
		{
			uint32_t v0 = a_data.indices[i + 0];
			uint32_t v1 = a_data.indices[i + 1];
			uint32_t v2 = a_data.indices[i + 2];

			uint32_t m01 = getMidpointIndex(v0, v1);
			uint32_t m12 = getMidpointIndex(v1, v2);
			uint32_t m20 = getMidpointIndex(v2, v0);

			subdividedData.indices.push_back(v0);
			subdividedData.indices.push_back(m01);
			subdividedData.indices.push_back(m20);

			subdividedData.indices.push_back(v1);
			subdividedData.indices.push_back(m12);
			subdividedData.indices.push_back(m01);

			subdividedData.indices.push_back(v2);
			subdividedData.indices.push_back(m20);
			subdividedData.indices.push_back(m12);

			subdividedData.indices.push_back(m01);
			subdividedData.indices.push_back(m12);
			subdividedData.indices.push_back(m20);
		}

		a_data = subdividedData;
		Subdivision(a_data, --a_subCount);
		logger::info("{} : {} subdivition done", __func__, a_data.vertices.size());
		return;
	}
	void TextureManager::VertexSmooth(GeometryData& a_data, float a_strength, std::uint32_t a_smoothCount)
	{
		if (a_smoothCount == 0)
			return;
		logger::info("{} : {} vertex smooth({})...", __func__, a_data.vertices.size(), a_smoothCount);
		std::vector<std::vector<uint32_t>> adjacency(a_data.vertices.size());

		for (size_t i = 0; i < a_data.indices.size(); i += 3)
		{
			uint32_t i0 = a_data.indices[i + 0];
			uint32_t i1 = a_data.indices[i + 1];
			uint32_t i2 = a_data.indices[i + 2];

			adjacency[i0].push_back(i1);
			adjacency[i0].push_back(i2);

			adjacency[i1].push_back(i0);
			adjacency[i1].push_back(i2);

			adjacency[i2].push_back(i0);
			adjacency[i2].push_back(i1);
		}

		for (auto& neighbors : adjacency)
		{
			std::sort(neighbors.begin(), neighbors.end());
			neighbors.erase(std::unique(neighbors.begin(), neighbors.end()), neighbors.end());
		}

		concurrency::concurrent_vector<DirectX::XMFLOAT3> smoothedVertices = a_data.vertices;
		for (size_t i = 0; i < a_data.vertices.size(); ++i)
		{
			const auto& v = a_data.vertices[i];
			if (adjacency[i].empty())
				continue;

			DirectX::XMFLOAT3 avg{ 0, 0, 0 };
			for (auto neighborIdx : adjacency[i])
			{
				const auto& nv = a_data.vertices[neighborIdx];
				avg.x += nv.x;
				avg.y += nv.y;
				avg.z += nv.z;
			}
			float invN = 1.0f / adjacency[i].size();
			avg.x *= invN;
			avg.y *= invN;
			avg.z *= invN;

			smoothedVertices[i].x = v.x * (1 - a_strength) + avg.x * a_strength;
			smoothedVertices[i].y = v.y * (1 - a_strength) + avg.y * a_strength;
			smoothedVertices[i].z = v.z * (1 - a_strength) + avg.z * a_strength;
		}
		a_data.vertices = smoothedVertices;
		VertexSmooth(a_data, --a_smoothCount);
		logger::info("{} : {} vertex smooth done", __func__, a_data.vertices.size());
		return;
	}
	bool TextureManager::ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out)
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
