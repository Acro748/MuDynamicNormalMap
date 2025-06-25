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
		std::string name = actor->GetName();
		std::uint32_t bipedSlot = RE::BIPED_OBJECT::kHead;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		if (Config::GetSingleton().GetBakeEnable())
		{
			RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, name, bipedSlot, delayTaskID]() {
				RE::Actor* actor = GetFormByID<RE::Actor*>(id);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so skip", id, name);
					return;
				}
				QBakeSkinObjectsNormalMap(actor, bipedSlot);
			});
		}
	}
	void TaskManager::onEvent(const ActorChangeHeadPartEvent& e) 
	{
		if (!e.actor)
			return;

		RE::FormID id = e.actor->formID;
		std::string name = e.actor->GetName();
		std::uint32_t bipedSlot = RE::BIPED_OBJECT::kHead;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		if (Config::GetSingleton().GetBakeEnable())
		{
			RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, name, bipedSlot, delayTaskID]() {
				RE::Actor* actor = GetFormByID<RE::Actor*>(id);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so skip", id, name);
					return;
				}
				QBakeSkinObjectsNormalMap(actor, bipedSlot);
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
		std::string name = e.actor->GetName();
		std::uint32_t bipedSlot = e.bipedSlot;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);

		if (Config::GetSingleton().GetBakeEnable())
		{
			RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, name, bipedSlot, delayTaskID]() {
				RE::Actor* actor = GetFormByID<RE::Actor*>(id);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x} {} : invalid reference. so skip", id, name);
					return;
				}
				QBakeSkinObjectsNormalMap(actor, bipedSlot);
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
			if (!arma->bipedModelData.bipedObjectSlots.any(RE::BIPED_MODEL::BipedObjectSlot(1 << bipedSlot)))
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
	std::unordered_set<RE::BSGeometry*> TaskManager::GetSkinGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot)
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
			if (!arma->bipedModelData.bipedObjectSlots.any(RE::BIPED_MODEL::BipedObjectSlot(1 << bipedSlot)))
				continue;
			char addonString[MAX_PATH]{ '\0' };
			arma->GetNodeName(addonString, a_actor, armo, -1);
			auto root = a_actor->loadedData->data3D->GetObjectByName(addonString);
			if (!root)
				continue;
			RE::BSVisit::TraverseScenegraphGeometries(root, [&geometries](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
				using State = RE::BSGeometry::States;
				using Feature = RE::BSShaderMaterial::Feature;
				if (!geometry || geometry->name.empty())
					return RE::BSVisit::BSVisitControl::kContinue;
				if (!geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect])
					return RE::BSVisit::BSVisitControl::kContinue;
				auto effect = geometry->GetGeometryRuntimeData().properties[State::kEffect].get();
				if (!effect)
					return RE::BSVisit::BSVisitControl::kContinue;
				auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
				if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
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

	void TaskManager::QBakeSkinObjectsNormalMap(RE::Actor* a_actor, std::uint32_t bipedSlot)
	{
		if (!a_actor)
			return;
		if (!Config::GetSingleton().GetBakeEnable())
			return;
		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
			return;
		if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
			return;

		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);
		RegisterDelayTask(delayTaskID, Config::GetSingleton().GetNormalmapBakeDelayTick(), [this, id, actorName, bipedSlot, delayTaskID]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				logger::error("{:x}::{} : invalid reference. so skip", id, actorName);
				return;
			}
			std::unordered_set<RE::BSGeometry*> geometries;
			if (bipedSlot == RE::BIPED_OBJECT::kHead || bipedSlot == RE::BIPED_OBJECT::kBody || bipedSlot == RE::BIPED_OBJECT::kHands || bipedSlot == RE::BIPED_OBJECT::kFeet)
			{
				geometries = GetSkinGeometries(actor, RE::BIPED_OBJECT(bipedSlot));
				if (geometries.empty())
					return;
				if (bipedSlot != RE::BIPED_OBJECT::kBody)
				{
					auto body = GetSkinGeometries(actor, RE::BIPED_OBJECT::kBody);
					geometries.insert(body.begin(), body.end());
				}
				if (bipedSlot != RE::BIPED_OBJECT::kHands)
				{
					auto hands = GetSkinGeometries(actor, RE::BIPED_OBJECT::kHands);
					geometries.insert(hands.begin(), hands.end());
				}
				if (bipedSlot != RE::BIPED_OBJECT::kFeet)
				{
					auto feet = GetSkinGeometries(actor, RE::BIPED_OBJECT::kFeet);
					geometries.insert(feet.begin(), feet.end());
				}
				if (Config::GetSingleton().GetHeadEnable() && bipedSlot != RE::BIPED_OBJECT::kFeet)
				{
					auto head = actor->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kFace);
					auto headGeo = head ? head->AsGeometry() : nullptr;
					if (headGeo)
						geometries.insert(headGeo);
				}
			}
			else
				geometries = GetSkinGeometries(actor, bipedSlot);
			QBakeObjectNormalMap(actor, geometries, bipedSlot);
		});
	}

	bool TaskManager::QBakeObjectNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot)
	{
		if (!a_actor || a_srcGeometies.empty() || bipedSlot >= RE::BIPED_OBJECT::kEditorTotal)
		{
			logger::error("{} : Invalid parameters", __func__);
			return false;
		}

		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		BakeData bakeData;
		std::size_t geoIndex = 0;
		for (auto& geo : a_srcGeometies)
		{
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (!geo || geo->name.empty())
				continue;
			if (!geo->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect])
				continue;
			auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
			auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
			if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
				continue;
			RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (!material || !material->normalTexture || !material->textureSet)
				continue;
			std::string texturePath = GetTexturePath(material->textureSet.get(), RE::BSTextureSet::Texture::kNormal);
			if (texturePath.empty())
				continue;
			if (!geo->GetGeometryRuntimeData().skinInstance)
				continue;
			auto skinInstance = geo->GetGeometryRuntimeData().skinInstance.get();
			auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance);
			if (!dismember)
				continue;
			auto slot = dismember->GetRuntimeData().partitions[0].slot - 30;

			std::string textureName = GetTextureName(a_actor, slot, geo);

			bakeData.geoData.GetGeometryData(geo);
			BakeTextureSet newBakeTextureSet;
			newBakeTextureSet.textureName = textureName;
			newBakeTextureSet.geometryName = geo->name.c_str();
			newBakeTextureSet.srcTexturePath = texturePath;
			newBakeTextureSet.overlayTexturePath = GetBakeNormalMapOverlayTexture(geo->name.c_str(), slot);
			bakeData.bakeTextureSet.emplace(geoIndex, newBakeTextureSet);
			logger::debug("{:x}::{} : {} - (vertices {} / uvs {} / tris {}) queue added on bake object normalmap", id, actorName,
						 geo->name.c_str(), newBakeTextureSet.overlayTexturePath, 
						 bakeData.geoData.geometries[geoIndex].second.vertexCount(), bakeData.geoData.geometries[geoIndex].second.uvCount(), bakeData.geoData.geometries[geoIndex].second.indicesCount() / 3);

			auto found = lastNormalMap[id].find(bakeData.geoData.geometries[geoIndex].second.vertexCount());
			if (found != lastNormalMap[id].end())
				Shader::TextureLoadManager::CreateSourceTexture(found->second, material->normalTexture);

			geoIndex++;
		}

		if (bipedSlot == RE::BIPED_OBJECT::kHead || bipedSlot == RE::BIPED_OBJECT::kBody || bipedSlot == RE::BIPED_OBJECT::kHands || bipedSlot == RE::BIPED_OBJECT::kFeet)
			bipedSlot = RE::BIPED_OBJECT::kBody;
		auto taskIDsrc = TaskID{ id, std::to_string(bipedSlot)};
		taskIDsrc.taskID = AttachBakeObjectNormalMapTaskID(taskIDsrc);

		std::thread([this, id, actorName, bakeData, taskIDsrc]() {
			SetDeferredWorker();

			struct textureResult {
				std::string geometryName;
				RE::NiPointer<RE::NiSourceTexture> texture;
			};

			if (GetCurrentBakeObjectNormalMapTaskID(taskIDsrc) != taskIDsrc.taskID)
			{
				logger::info("{:x}::{}::{} : cancel queue for bake object normalmap", id, actorName, taskIDsrc.taskID);
				return;
			}
			auto textures = ObjectNormalMapBaker::GetSingleton().BakeObjectNormalMap(taskIDsrc, bakeData.geoData, bakeData.bakeTextureSet);
			if (textures.empty())
			{
				logger::error("{:x}::{}::{} : Failed to bake object normalmap", id, actorName, taskIDsrc.taskID);
				return;
			}

			RegisterDelayTask(std::to_string(GenerateUniqueID()), [this, id, actorName, textures, taskIDsrc]() {
				auto refr = GetFormByID<RE::TESObjectREFR*>(id);
				if (!refr || !refr->loadedData || !refr->loadedData->data3D)
				{
					logger::error("{:x}::{}::{} : invalid reference. so cancel all current queue for bake object normalmap", id, actorName, taskIDsrc.taskID);
					DetachBakeObjectNormalMapTaskID(taskIDsrc, taskIDsrc.taskID);
					return;
				}
				auto root = refr->loadedData->data3D.get();

				for (auto& result : textures)
				{
					using State = RE::BSGeometry::States;
					using Feature = RE::BSShaderMaterial::Feature;
					if (GetCurrentBakeObjectNormalMapTaskID(taskIDsrc) != taskIDsrc.taskID)
					{
						logger::error("{:x}::{}::{} : invalid task queue. so cancel all current queue for bake object normalmap", id, actorName, taskIDsrc.taskID);
						return;
					}
					auto obj = root->GetObjectByName(result.geoName.c_str());
					auto geo = obj ? obj->AsGeometry() : nullptr;
					if (!geo)
					{
						logger::error("{:x}::{}::{} : {} invalid geometry. so cancel bake object normalmap for this geoemtry", id, actorName, taskIDsrc.taskID, result.geoName.c_str());
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

					material->normalTexture = result.normalmap;

					lastNormalMap[id][result.vertexCount] = result.textureName;
					logger::info("{:x}::{}::{} : {} bake object normalmap done", id, actorName, taskIDsrc.taskID, geo->name.c_str());
				}
			});
		}).detach();
		return true;
	}

	std::string TaskManager::GetBakeNormalMapOverlayTexture(std::string a_geometryName, std::uint32_t bipedSlot)
	{
		const std::filesystem::path baseFolder = "Textures\\MuDynamicTextureTool\\BakeNormalMap\\Overlay";
		const std::filesystem::path defaultMaskPath = baseFolder / "Default.dds";
		if (a_geometryName.empty() || bipedSlot >= RE::BIPED_OBJECT::kEditorTotal)
			return defaultMaskPath.string();

		bipedSlot += 30;
		std::filesystem::path overlayPath = "Data" / baseFolder / std::to_string(bipedSlot);
		if (IsExistDirectoy(overlayPath.string()))
		{
			try {
				for (const auto& file : std::filesystem::directory_iterator(overlayPath))
				{
					if (IsContainString(a_geometryName, file.path().stem().string()))
						return (baseFolder / std::to_string(bipedSlot) / file.path().filename()).string();
				}
			}
			catch (...) {}
			return (baseFolder / std::to_string(bipedSlot) / "Default.dds").string();
		}
		return defaultMaskPath.string();
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
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return -1;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		auto taskID = GenerateUniqueID();
		bakeObjectNormalMapCounter[taskIDsrc.refrID][taskIDsrc.taskName] = taskID;
		taskIDsrc.taskID = taskID;
		return taskID;
	}
	void TaskManager::DetachBakeObjectNormalMapTaskID(TaskID taskIDsrc, std::int64_t a_ownID) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		if (bakeObjectNormalMapCounter[taskIDsrc.refrID][taskIDsrc.taskName] == a_ownID)
			bakeObjectNormalMapCounter[taskIDsrc.refrID].erase(taskIDsrc.taskName);
	}
	void TaskManager::ReleaseBakeObjectNormalMapTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		bakeObjectNormalMapCounter[taskIDsrc.refrID].erase(taskIDsrc.taskName);
	}
	std::uint64_t TaskManager::GetCurrentBakeObjectNormalMapTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return -1;
		std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
		auto taskID = bakeObjectNormalMapCounter[taskIDsrc.refrID][taskIDsrc.taskName];
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
