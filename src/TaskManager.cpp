#include "TaskManager.h"

namespace Mus {
	void TaskManager::Init()
	{
	}

	void TaskManager::onEvent(const FrameEvent& e)
	{
		RunDelayTask();
	}
	void TaskManager::onEvent(const FacegenNiNodeEvent& e) 
	{
		if (!e.facegenNiNode)
			return;
		RE::Actor* actor = skyrim_cast<RE::Actor*>(e.facegenNiNode->GetUserData());
		if (!actor)
			return;

		RE::FormID id = actor->formID;
		std::uint32_t bipedSlot = RE::BIPED_OBJECT::kHead;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		if (Config::GetSingleton().GetBakeEnable() && Config::GetSingleton().GetHeadEnable())
		{
			if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
				return;
			if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
				return;
			RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, bipedSlot, delayTaskID]() {
				RE::Actor* actor = GetFormByID<RE::Actor*>(id);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so skip", id, GetBipedName(bipedSlot));
					return;
				}
				std::unordered_set<RE::BSGeometry*> geometries = GetGeometries(actor->GetFaceNode(), [](RE::BSGeometry*) -> bool { return true; });
				QBakeObjectNormalMap(actor, geometries, bipedSlot);
			});
		}
	}
	void TaskManager::onEvent(const ActorChangeHeadPartEvent& e) 
	{
		if (!e.actor)
			return;

		RE::FormID id = e.actor->formID;
		std::uint32_t bipedSlot = RE::BIPED_OBJECT::kHead;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		if (Config::GetSingleton().GetBakeEnable() && Config::GetSingleton().GetHeadEnable())
		{
			if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
				return;
			if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
				return;
			RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, bipedSlot, delayTaskID]() {
				RE::Actor* actor = GetFormByID<RE::Actor*>(id);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so skip", id, GetBipedName(bipedSlot));
					return;
				}
				std::unordered_set<RE::BSGeometry*> geometries = GetGeometries(actor->GetFaceNode(), [](RE::BSGeometry*) -> bool { return true; });
				QBakeObjectNormalMap(actor, geometries, bipedSlot);
			});
		}
	}
	void TaskManager::onEvent(const ArmorAttachEvent& e)
	{
		if (!e.actor)
			return;
		if (!isPlayer(e.actor->formID))
			return;
		if (!e.hasAttached)
			return;

		RE::FormID id = e.actor->formID;
		std::uint32_t bipedSlot = e.bipedSlot;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		if (Config::GetSingleton().GetBakeEnable())
		{
			if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
				return;
			if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
				return;
			/*for (auto geo : GetGeometries(e.attachedNode, [](RE::BSGeometry* a_geo) -> bool { return true; }))
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

				RE::BSTriShape* triShape = geo->AsTriShape();
				if (!triShape)
					continue;
				std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
				RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(geo);
				if (!skinPartition)
					continue;
				vertexCount = vertexCount ? vertexCount : skinPartition->vertexCount;

				auto found = lastNormalMap[id].find(vertexCount);
				if (found == lastNormalMap[id].end())
					continue;
				if (Shader::TextureLoadManager::CreateSourceTexture(found->second, material->normalTexture) == 0)
				{
					material->textureSet->SetTexturePath(RE::BSTextureSet::Texture::kNormal, tempTexture.c_str());
				}
			}*/

			RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, bipedSlot, delayTaskID]() {
				RE::Actor* actor = GetFormByID<RE::Actor*>(id);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so skip", id, GetBipedName(bipedSlot));
					return;
				}
				std::unordered_set<RE::BSGeometry*> geometries = GetGeometries(actor, bipedSlot);
				QBakeObjectNormalMap(actor, geometries, bipedSlot);
			});
		}
	}

	void TaskManager::RunDelayTask()
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
	void TaskManager::RegisterDelayTask(std::string id, std::function<void()> func) 
	{
		std::lock_guard<std::mutex> lg(delayTaskLock);
		delayTask[id] = func;
	}
	void TaskManager::RegisterDelayTask(std::string id, std::uint8_t delayTick, std::function<void()> func) 
	{
		if (delayTick == 0)
			RegisterDelayTask(id, func);
		else
			RegisterDelayTask(id, --delayTick, func);
	}
	std::string TaskManager::GetDelayTaskID(RE::FormID refrID, std::uint32_t bipedSlot) 
	{
		return std::to_string(refrID) + "_" + std::to_string(bipedSlot);
	}

	std::unordered_set<RE::BSGeometry*> Mus::TaskManager::GetGeometries(RE::NiAVObject* a_root, std::function<bool(RE::BSGeometry*)> func)
	{
		std::unordered_set<RE::BSGeometry*> geometries;
		if (!a_root)
			return geometries;
		RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
			if (func(geo))
				geometries.insert(geo);
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return geometries;
	}
	std::unordered_set<RE::BSGeometry*> TaskManager::GetGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot)
	{
		std::unordered_set<RE::BSGeometry*> geometries;
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
				geometries.insert(geometry);
				return RE::BSVisit::BSVisitControl::kContinue;
			});
			if (!geometries.empty())
				continue;
		}
		return geometries;
	}
	std::unordered_set<RE::BSGeometry*> TaskManager::GetGeometries(std::string a_fileName)
	{
		std::unordered_set<RE::BSGeometry*> geometries;

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
					geometries.insert(geo);
				}
			}
		}
		NiStreamDtor(niStream);
		return geometries;
	}

	bool TaskManager::QBakeObjectNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot)
	{
		std::string bipedSlotName = GetBipedName(bipedSlot);
		if (!a_actor || a_srcGeometies.empty() || bipedSlot >= RE::BIPED_OBJECT::kEditorTotal)
		{
			logger::error("{} {} : Invalid parameters", __func__, bipedSlotName);
			return false;
		}

		RE::FormID id = a_actor->formID;
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

			std::string textureName = GetTextureName(a_actor, bipedSlot, geo);
			BakeData bakeData{
				geo->name.c_str(),
				textureName,
				GeometryData(geo),
				texturePath,
				GetCustomBakeNormalMapMaskTexture(a_actor, geo->name.c_str(), bipedSlot)
			};
			logger::info("{:x} {} {} : {} {} (vertices {} / uvs {} / tris {}) queue added on bake object normalmap", a_actor->formID, a_actor->GetName(), bipedSlotName,
						 geo->name.c_str(), bakeData.maskTexturePath, bakeData.data.vertices.size(), bakeData.data.uvs.size(), bakeData.data.indices.size() / 3);

			auto found = lastNormalMap[id].find(bakeData.data.vertices.size());
			if (found != lastNormalMap[id].end())
				Shader::TextureLoadManager::CreateSourceTexture(found->second, material->normalTexture);

			auto taskIDsrc = TaskID{ id, geo->name.c_str() };
			taskIDsrc.taskID = AttachBakeObjectNormalMapTaskID(taskIDsrc);

			std::thread([this, id, bakeData, bipedSlotName, bipedSlot, taskIDsrc]() {
				SetDeferredWorker();

				if (GetCurrentBakeObjectNormalMapTaskID(taskIDsrc) != taskIDsrc.taskID)
				{
					logger::info("{:x} {} : {} cancel queue for bake object normalmap", id, bakeData.geometryName, bipedSlotName);
					return;
				}
				auto texture = ObjectNormalMapBaker::GetSingleton().BakeObjectNormalMap(taskIDsrc, bakeData.textureName, bakeData.data, bakeData.srcTexturePath, bakeData.maskTexturePath);
				if (!texture)
				{
					logger::error("{:x} {} : {} Failed to bake object normalmap", id, bakeData.geometryName, bipedSlotName);
					return;
				}

				lastNormalMap[id][bakeData.data.vertices.size()] = bakeData.textureName;

				struct textureResult {
					std::string geometryName;
					RE::NiPointer<RE::NiSourceTexture> texture;
				};
				textureResult result;
				result.geometryName = bakeData.geometryName;
				result.texture = texture;

				RegisterDelayTask(std::to_string(GenerateUniqueID()), [this, id, result, bipedSlotName, bipedSlot, taskIDsrc]() {
					auto refr = GetFormByID<RE::TESObjectREFR*>(id);
					if (!refr || !refr->loadedData || !refr->loadedData->data3D)
					{
						logger::error("{:x} {} : invalid reference. so cancel all current queue for bake object normalmap", id, bipedSlotName);
						DetachBakeObjectNormalMapTaskID(taskIDsrc, taskIDsrc.taskID);
						return;
					}
					auto root = refr->loadedData->data3D.get();

					using State = RE::BSGeometry::States;
					using Feature = RE::BSShaderMaterial::Feature;
					if (GetCurrentBakeObjectNormalMapTaskID(taskIDsrc) != taskIDsrc.taskID)
					{
						logger::error("{:x} {} {} : invalid task queue. so cancel all current queue for bake object normalmap", id, refr->GetName(), bipedSlotName);
						return;
					}
					auto obj = root->GetObjectByName(result.geometryName.c_str());
					auto geo = obj ? obj->AsGeometry() : nullptr;
					if (!geo)
					{
						logger::error("{:x} {} {} : {} invalid geometry. so cancel bake object normalmap for this geoemtry", id, refr->GetName(), result.geometryName.c_str(), bipedSlotName);
						return;
					}
					auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
					if (!effect)
						return;
					auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
					if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
						return;
					RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
					if (!material || !material->normalTexture)
						return;
					material->normalTexture = result.texture;
					logger::info("{:x} {} {} : {} bake object normalmap done", id, refr->GetName(), geo->name.c_str(), bipedSlotName);
				});
			}).detach();
		}

		return true;
	}

	std::string TaskManager::GetBakeNormalMapMaskTexture(std::string a_geometryName, std::uint32_t bipedSlot, std::filesystem::path baseFolder)
	{
		const std::filesystem::path defaultMaskPath = baseFolder / "Default.dds";
		if (a_geometryName.empty() || bipedSlot >= RE::BIPED_OBJECT::kEditorTotal)
			return defaultMaskPath.string();

		bipedSlot += 30;
		std::filesystem::path maskPath = "Data" / baseFolder / std::to_string(bipedSlot);
		if (IsExistDirectoy(maskPath.string()))
		{
			try {
				for (const auto& file : std::filesystem::directory_iterator(maskPath))
				{
					if (IsContainString(a_geometryName, file.path().stem().string()))
						return stringRemoveStarts(file.path().string(), GetRuntimeDataDirectory());
				}
			}
			catch (...) {}
			return (maskPath / "Default.dds").string();
		}
		return baseFolder.string();
	}
	std::string TaskManager::GetCustomBakeNormalMapMaskTexture(RE::Actor* a_actor, std::string a_geometryName, std::uint32_t bipedSlot)
	{
		if (!a_actor || a_geometryName.empty() || bipedSlot >= RE::BIPED_OBJECT::kEditorTotal)
			return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot);

		if (bipedSlot == RE::BIPED_OBJECT::kHead)
		{
			if (auto hp = RE::TESForm::LookupByEditorID<RE::BGSHeadPart>(a_geometryName); hp)
			{
				if (auto found = bakeObjectNormalMapMaskTexture.find(hp->formID); found != bakeObjectNormalMapMaskTexture.end())
					return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot, found->second);
			}
		}
		if (auto armor = a_actor->GetSkin(RE::BIPED_MODEL::BipedObjectSlot(1 << bipedSlot)); armor)
		{
			if (auto found = bakeObjectNormalMapMaskTexture.find(armor->formID); found != bakeObjectNormalMapMaskTexture.end())
				return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot, found->second);
		}
		if (auto race = a_actor->GetRace(); race)
		{
			if (auto found = bakeObjectNormalMapMaskTexture.find(race->formID); found != bakeObjectNormalMapMaskTexture.end())
				return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot, found->second);
		}
		if (auto actorBase = a_actor->GetActorBase(); actorBase)
		{
			if (auto found = bakeObjectNormalMapMaskTexture.find(actorBase->formID); found != bakeObjectNormalMapMaskTexture.end())
				return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot, found->second);
			if (actorBase->faceNPC)
			{
				if (auto found = bakeObjectNormalMapMaskTexture.find(actorBase->faceNPC->formID); found != bakeObjectNormalMapMaskTexture.end())
					return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot, found->second);
			}
		}
		if (auto found = bakeObjectNormalMapMaskTexture.find(a_actor->formID); found != bakeObjectNormalMapMaskTexture.end())
			return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot, found->second);
		return GetBakeNormalMapMaskTexture(a_geometryName, bipedSlot);
	}	
	void TaskManager::InsertCustomBakeNormalMapMaskTexture(RE::FormID id, std::string baseFolder)
	{
		baseFolder = stringRemoveStarts(baseFolder, GetRuntimeDataDirectory());
		if (id == 0 || baseFolder.empty() || !stringStartsWith(baseFolder, "Textures\\"))
			return;
		bakeObjectNormalMapMaskTexture[id] = baseFolder;
	}

	std::int64_t TaskManager::GenerateUniqueID()
	{
		static std::atomic<std::uint64_t> counter{ 0 };
		auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
		return (now << 16) | (counter++ & 0xFFFF);
	}
	std::uint64_t TaskManager::AttachBakeObjectNormalMapTaskID(TaskID& taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.geometryName.empty())
			return -1;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		auto taskID = GenerateUniqueID();
		bakeObjectNormalMapCounter[taskIDsrc.refrID][taskIDsrc.geometryName] = taskID;
		taskIDsrc.taskID = taskID;
		return taskID;
	}
	void TaskManager::DetachBakeObjectNormalMapTaskID(TaskID taskIDsrc, std::int64_t a_ownID) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.geometryName.empty())
			return;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		if (bakeObjectNormalMapCounter[taskIDsrc.refrID][taskIDsrc.geometryName] == a_ownID)
			bakeObjectNormalMapCounter[taskIDsrc.refrID].erase(taskIDsrc.geometryName);
	}
	void TaskManager::ReleaseBakeObjectNormalMapTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.geometryName.empty())
			return;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		bakeObjectNormalMapCounter[taskIDsrc.refrID].erase(taskIDsrc.geometryName);
	}
	std::uint64_t TaskManager::GetCurrentBakeObjectNormalMapTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.geometryName.empty())
			return -1;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		auto taskID = bakeObjectNormalMapCounter[taskIDsrc.refrID][taskIDsrc.geometryName];
		if (taskID != taskIDsrc.taskID)
			return -1;
		return taskID;
	}

	std::string TaskManager::GetTextureName(RE::Actor* a_actor, std::uint32_t a_bipedSlot, RE::BSGeometry* a_geo)
	{ // ActorID + Armor/SkinID + BipedSlot + GeometryName + VertexCount
		if (!a_actor || !a_geo || a_geo->name.empty())
			return "";
		std::uint32_t vertexCount = 0;
		RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (triShape)
			vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		auto& runtimeData = a_geo->GetGeometryRuntimeData();
		if (runtimeData.skinInstance && runtimeData.skinInstance->skinPartition)
			vertexCount ? vertexCount : runtimeData.skinInstance->skinPartition->vertexCount;
		return GetHexStr(a_actor->formID) + "_" + std::to_string(a_bipedSlot) + "_" + a_geo->name.c_str() + "_" + std::to_string(vertexCount);
	}
	bool TaskManager::GetTextureInfo(std::string a_textureName, TextureInfo& a_textureInfo)
	{ // ActorID + GeometryName + VertexCount
		if (a_textureName.empty())
			return false;
		auto frag = split(a_textureName, '_');
		if (frag.size() < 5)
			return false;
		a_textureInfo.actorID = GetHex(frag[0]);
		a_textureInfo.armorID = GetHex(frag[1]);
		a_textureInfo.bipedSlot = Config::GetUIntValue(frag[2]);
		a_textureInfo.geoName = frag[3];
		a_textureInfo.vertexCount = Config::GetIntValue(frag[4]);
		return true;
	}
}
