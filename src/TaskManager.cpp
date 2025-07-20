#include "TaskManager.h"

namespace Mus {
	void TaskManager::Init(bool dataLoaded)
	{
		if (!dataLoaded)
		{
			if (const auto NiNodeEvent = SKSE::GetNiNodeUpdateEventSource(); NiNodeEvent)
				NiNodeEvent->AddEventSink<SKSE::NiNodeUpdateEvent>(this);
		}
		else
		{
			if (auto inputManager = RE::BSInputDeviceManager::GetSingleton(); inputManager)
				inputManager->AddEventSink<RE::InputEvent*>(this);
		}
	}

	void TaskManager::onEvent(const FrameEvent& e)
	{
		if (IsSaveLoading.load())
			return;

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
		std::uint32_t bipedSlot = BipedObjectSlot::kHead;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);
		RegisterDelayTask(delayTaskID, Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, bipedSlot, delayTaskID]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
			QUpdateNormalMap(actor, bipedSlot);
		});
		//ActorVertexHasher::GetSingleton().Register(actor, RE::BIPED_OBJECT::kHead);
	}
	void TaskManager::onEvent(const ActorChangeHeadPartEvent& e)
	{
		if (!e.actor)
			return;

		RE::FormID id = e.actor->formID;
		std::string name = e.actor->GetName();
		std::uint32_t bipedSlot = BipedObjectSlot::kHead;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);
		RegisterDelayTask(delayTaskID, Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, bipedSlot, delayTaskID]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
			QUpdateNormalMap(actor, bipedSlot);
		});
		//ActorVertexHasher::GetSingleton().Register(e.actor, RE::BIPED_OBJECT::kHead);

	}
	void TaskManager::onEvent(const ArmorAttachEvent& e)
	{
		if (!e.actor)
			return;
		if (!e.hasAttached)
			return;

		RE::FormID id = e.actor->formID;
		std::string name = e.actor->GetName();
		std::uint32_t bipedSlot = 1 << e.bipedSlot;
		std::string delayTaskID = GetDelayTaskID(id, bipedSlot);
		RegisterDelayTask(delayTaskID, Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, bipedSlot, delayTaskID]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
			QUpdateNormalMap(actor, bipedSlot);
		});
		//ActorVertexHasher::GetSingleton().Register(e.actor, RE::BIPED_OBJECT(e.bipedSlot));
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

	std::unordered_set<RE::BSGeometry*> TaskManager::GetAllGeometries(RE::Actor* a_actor)
	{
		std::unordered_set<RE::BSGeometry*> geometries;
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return geometries;
		auto root = a_actor->loadedData->data3D.get();
		if (!root)
			return geometries;
		RE::BSVisit::TraverseScenegraphGeometries(root, [&geometries](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (!geometry || geometry->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (IsContainString(geometry->name.c_str(), "[Ovl") || IsContainString(geometry->name.c_str(), "[SOvl") || IsContainString(geometry->name.c_str(), "overlay")) //without overlay
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
		return geometries;
	}
	std::unordered_set<RE::BSGeometry*> TaskManager::GetGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot)
	{
		std::unordered_set<RE::BSGeometry*> geometries;
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return geometries;
		auto slots = SubBipedObjectSlots(bipedSlot);
		auto root = a_actor->loadedData->data3D.get();
		if (!root)
			return geometries;
		bool isHeadInclude = std::find(slots.begin(), slots.end(), std::to_underlying(BipedObjectSlot::kHead)) != slots.end();
		RE::BSVisit::TraverseScenegraphGeometries(root, [&geometries, &slots, isHeadInclude](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (!geometry || geometry->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (IsContainString(geometry->name.c_str(), "[Ovl") || IsContainString(geometry->name.c_str(), "[SOvl") || IsContainString(geometry->name.c_str(), "overlay")) //without overlay
				return RE::BSVisit::BSVisitControl::kContinue;
			if (!geometry->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect])
				return RE::BSVisit::BSVisitControl::kContinue;
			auto effect = geometry->GetGeometryRuntimeData().properties[State::kEffect].get();
			if (!effect)
				return RE::BSVisit::BSVisitControl::kContinue;
			auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
			if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
				return RE::BSVisit::BSVisitControl::kContinue;

			if (geometry->AsDynamicTriShape() && isHeadInclude)
			{
				geometries.insert(geometry);
			}
			else
			{
				std::uint32_t pslot = 0;
				auto skinInstance = geometry->GetGeometryRuntimeData().skinInstance.get();
				auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance);
				if (!dismember) //maybe it's just skinInstance in headpart
					return RE::BSVisit::BSVisitControl::kContinue;
				bool found = false;
				for (std::uint32_t p = 0; p < dismember->GetRuntimeData().numPartitions; p++)
				{
					auto& partition = dismember->GetRuntimeData().partitions[p];
					if (partition.slot < 30 || partition.slot >= RE::BIPED_OBJECT::kEditorTotal + 30)
						return RE::BSVisit::BSVisitControl::kContinue; //unknown slot
					else
						pslot = 1 << (partition.slot - 30);
					if (std::find(slots.begin(), slots.end(), pslot) != slots.end())
					{
						found = true;
						break;
					}
				}
				if (!found)
					return RE::BSVisit::BSVisitControl::kContinue;
				geometries.insert(geometry);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return geometries;
	}

	void TaskManager::QUpdateNormalMap(RE::Actor* a_actor, std::uint32_t bipedSlot)
	{
		if (!a_actor)
			return;
		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
			return;
		if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
			return;
		if (GetSex(a_actor) == RE::SEX::kMale && !Config::GetSingleton().GetMaleEnable())
			return;
		if (GetSex(a_actor) == RE::SEX::kFemale && !Config::GetSingleton().GetFemaleEnable())
			return;
		if (!Config::GetSingleton().GetHeadEnable())
			return;
		auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
		if (!condition.Enable)
			return;
		if (!condition.HeadEnable)
			bipedSlot &= ~std::to_underlying(BipedObjectSlot::kHead);
		QUpdateNormalMap(a_actor, GetAllGeometries(a_actor), bipedSlot);
	}

	bool TaskManager::QUpdateNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot)
	{
		if (!a_actor || a_srcGeometies.empty() || bipedSlot == 0)
		{
			logger::error("{} : Invalid parameters", __func__);
			return false;
		}

		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		BakeData bakeData;
		std::size_t geoIndex = 0;
		auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
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

			bakeData.geoData.CopyGeometryData(geo);
			std::uint32_t slot = 0;
			auto skinInstance = geo->GetGeometryRuntimeData().skinInstance.get();
			auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance);
			if (dismember)
			{
				if (dismember->GetRuntimeData().partitions[0].slot < 30 || dismember->GetRuntimeData().partitions[0].slot >= RE::BIPED_OBJECT::kEditorTotal + 30)
				{
					if (auto dynamicTri = geo->AsDynamicTriShape(); dynamicTri) { //maybe head
						slot = RE::BIPED_OBJECT::kHead;
					}
					else //unknown slot
						continue;
				}
				else
					slot = dismember->GetRuntimeData().partitions[0].slot - 30;
			}
			else //maybe it's just skinInstance in headpart
			{
				slot = RE::BIPED_OBJECT::kHead;
			}

			if (bipedSlot & 1 << slot)
			{
				BakeTextureSet newBakeTextureSet;
				newBakeTextureSet.geometry = geo;
				newBakeTextureSet.textureName = GetTextureName(a_actor, slot, geo);
				newBakeTextureSet.geometryName = geo->name.c_str();
				newBakeTextureSet.srcTexturePath = GetTangentNormalMapPath(texturePath, condition.ProxyTangentTextureFolder);
				newBakeTextureSet.overlayTexturePath = GetOverlayNormalMapPath(texturePath, condition.ProxyOverlayTextureFolder);
				bakeData.bakeTextureSet.emplace(geoIndex, newBakeTextureSet);
				logger::debug("{:x}::{} : {} - queue added on bake object normalmap", id, actorName,
							  geo->name.c_str(), newBakeTextureSet.overlayTexturePath);

				auto found = lastNormalMap[id].find(bakeData.geoData.geometries[geoIndex].second.info.vertexCount);
				if (found != lastNormalMap[id].end())
					Shader::TextureLoadManager::CreateSourceTexture(found->second, material->normalTexture);

				ActorVertexHasher::GetSingleton().Register(a_actor, RE::BIPED_OBJECT(slot));
			}
			geoIndex++;
		}

		if (bakeData.bakeTextureSet.empty())
			return false;

		auto taskIDsrc = TaskID{ id, std::to_string(bipedSlot)};
		taskIDsrc.taskID = AttachTaskID(taskIDsrc);

		QUpdateNormalMap(taskIDsrc, id, actorName, bakeData);
		return true;
	}
	bool TaskManager::QUpdateNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::unordered_set<RE::BSGeometry*> a_updateTargets)
	{
		if (!a_actor || a_srcGeometies.empty() || a_updateTargets.empty())
		{
			logger::error("{} : Invalid parameters", __func__);
			return false;
		}

		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		BakeData bakeData;
		std::size_t geoIndex = 0;
		std::uint32_t bipedSlot = 0;
		auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
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

			bakeData.geoData.CopyGeometryData(geo);
			std::uint32_t slot = 0;
			auto skinInstance = geo->GetGeometryRuntimeData().skinInstance.get();
			auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance);
			if (dismember)
			{
				if (dismember->GetRuntimeData().partitions[0].slot < 30 || dismember->GetRuntimeData().partitions[0].slot >= RE::BIPED_OBJECT::kEditorTotal + 30)
				{
					if (auto dynamicTri = geo->AsDynamicTriShape(); dynamicTri) { //maybe head
						slot = RE::BIPED_OBJECT::kHead;
					}
					else //unknown slot
						continue;
				}
				else
					slot = dismember->GetRuntimeData().partitions[0].slot - 30;
			}
			else //maybe it's just skinInstance in headpart
			{
				slot = RE::BIPED_OBJECT::kHead;
			}

			if (a_updateTargets.find(geo) != a_updateTargets.end())
			{
				if (!(bipedSlot & 1 << slot))
				{
					bipedSlot += 1 << slot;
				}
				BakeTextureSet newBakeTextureSet;
				newBakeTextureSet.geometry = geo;
				newBakeTextureSet.textureName = GetTextureName(a_actor, slot, geo);
				newBakeTextureSet.geometryName = geo->name.c_str();
				newBakeTextureSet.srcTexturePath = GetTangentNormalMapPath(texturePath, condition.ProxyTangentTextureFolder);
				newBakeTextureSet.overlayTexturePath = GetOverlayNormalMapPath(texturePath, condition.ProxyOverlayTextureFolder);
				bakeData.bakeTextureSet.emplace(geoIndex, newBakeTextureSet);
				logger::debug("{:x}::{} : {} - queue added on bake object normalmap", id, actorName,
							  geo->name.c_str(), newBakeTextureSet.overlayTexturePath);

				auto found = lastNormalMap[id].find(bakeData.geoData.geometries[geoIndex].second.info.vertexCount);
				if (found != lastNormalMap[id].end())
					Shader::TextureLoadManager::CreateSourceTexture(found->second, material->normalTexture);

				ActorVertexHasher::GetSingleton().Register(a_actor, RE::BIPED_OBJECT(slot));
			}
			geoIndex++;
		}

		if (bakeData.bakeTextureSet.empty())
			return false;

		auto taskIDsrc = TaskID{ id, std::to_string(bipedSlot)};
		taskIDsrc.taskID = AttachTaskID(taskIDsrc);

		QUpdateNormalMap(taskIDsrc, id, actorName, bakeData);
		return true;
	}
	void TaskManager::QUpdateNormalMap(TaskID& taskIDsrc, RE::FormID& id, std::string& actorName, BakeData& bakeData)
	{
		actorThreads->submitAsync([this, id, actorName, bakeData, taskIDsrc]() {
			if (!IsValidTaskID(taskIDsrc))
			{
				logger::info("{:x}::{}::{} : cancel queue for bake object normalmap", id, actorName, taskIDsrc.taskID);
				return;
			}
			ObjectNormalMapUpdater::BakeResult textures;
			if (Config::GetSingleton().GetGPUEnable())
				textures = ObjectNormalMapUpdater::GetSingleton().UpdateObjectNormalMapGPU(taskIDsrc, bakeData.geoData, bakeData.bakeTextureSet);
			else
				textures = ObjectNormalMapUpdater::GetSingleton().UpdateObjectNormalMap(taskIDsrc, bakeData.geoData, bakeData.bakeTextureSet);
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
					DetachTaskID(taskIDsrc, taskIDsrc.taskID);
					return;
				}
				if (!IsValidTaskID(taskIDsrc))
				{
					logger::error("{:x}::{}::{} : invalid task queue. so cancel all current queue for bake object normalmap", id, actorName, taskIDsrc.taskID);
					return;
				}
				auto root = refr->loadedData->data3D.get();

				RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
					using State = RE::BSGeometry::States;
					using Feature = RE::BSShaderMaterial::Feature;
					if (!geo || geo->name.empty())
						return RE::BSVisit::BSVisitControl::kContinue;
					auto found = std::find_if(textures.begin(), textures.end(), [&geo](ObjectNormalMapUpdater::NormalMapResult normalmap) {
						return normalmap.geometry == geo;
					});
					if (found == textures.end())
						return RE::BSVisit::BSVisitControl::kContinue;
					auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
					if (!effect)
						return RE::BSVisit::BSVisitControl::kContinue;
					auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
					if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
						return RE::BSVisit::BSVisitControl::kContinue;
					RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
					if (!material || !material->normalTexture)
						return RE::BSVisit::BSVisitControl::kContinue;
					//material->textureSet->SetTexturePath(RE::BSTextureSet::Texture::kNormal, found->textureName.c_str());
					//material->diffuseTexture = found->normalmap;
					material->normalTexture = found->normalmap;
					lastNormalMap[id][found->vertexCount] = found->textureName;
					logger::info("{:x}::{}::{} : {} bake object normalmap done", id, actorName, taskIDsrc.taskID, geo->name.c_str());
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			});
		});
	}

	std::string TaskManager::GetTangentNormalMapPath(std::string a_normalMapPath)
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
			std::string fullPath = (file.parent_path() / (filename + ".dds")).string();
			if (IsExistFile(fullPath, ExistType::textures))
				return fullPath;
		}
		else if (stringEndsWith(filename, n_suffix.data())) //_n
		{
			if (IsExistFile(a_normalMapPath, ExistType::textures))
				return a_normalMapPath;
		}
		return "";
	}
	std::string TaskManager::GetTangentNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder)
	{
		std::string result = GetTangentNormalMapPath(a_normalMapPath);
		if (!result.empty() || a_proxyFolder.empty())
			return result;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			result = GetTangentNormalMapPath(folder + "\\" + filename + ".dds");
			if (!result.empty())
				return result;
		}
		return a_normalMapPath;
	}

	std::string TaskManager::GetOverlayNormalMapPath(std::string a_normalMapPath)
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
			std::string filename_n_ov = stringRemoveEnds(filename, msn_suffix.data());
			filename_n_ov += n_suffix;
			filename_n_ov += ov_suffix;
			std::string fullPath = (file.parent_path() / (filename_n_ov + ".dds")).string();
			if (IsExistFile(fullPath, ExistType::textures))
				return fullPath;
		}
		if (stringEndsWith(filename, n_suffix.data())) //_n -> _n_ov
		{
			std::string filename_n_ov = filename + ov_suffix.data();
			std::string fullPath = (file.parent_path() / (filename_n_ov + ".dds")).string();
			if (IsExistFile(fullPath, ExistType::textures))
				return fullPath;
		}
		if (stringEndsWith(filename, ov_suffix.data())) //_ov
		{
			if (IsExistFile(a_normalMapPath, ExistType::textures))
				return a_normalMapPath;
		}
		return "";
	}
	std::string TaskManager::GetOverlayNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder)
	{
		std::string result = GetOverlayNormalMapPath(a_normalMapPath);
		if (!result.empty() || a_proxyFolder.empty())
			return result;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			a_normalMapPath = folder + "\\" + filename + ".dds";
			result = GetOverlayNormalMapPath(a_normalMapPath);
			if (!result.empty())
				return result;
		}
		return "";
	}

	std::int64_t TaskManager::GenerateUniqueID()
	{
		static std::atomic<std::uint64_t> counter{ 0 };
		auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
		return (now << 16) | (counter++ & 0xFFFF);
	}
	std::uint64_t TaskManager::AttachTaskID(TaskID& taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return -1;
		auto taskID = GenerateUniqueID();
		updateNormalMapLock.lock();
		updateNormalMapTaskID[taskIDsrc.refrID][taskIDsrc.taskName] = taskID;
		updateNormalMapLock.unlock();
		taskIDsrc.taskID = taskID;
		return taskID;
	}
	void TaskManager::DetachTaskID(TaskID taskIDsrc, std::int64_t a_ownID) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return;
		updateNormalMapLock.lock();
		if (updateNormalMapTaskID[taskIDsrc.refrID][taskIDsrc.taskName] == a_ownID)
			updateNormalMapTaskID[taskIDsrc.refrID].erase(taskIDsrc.taskName);
		updateNormalMapLock.unlock();
	}
	void TaskManager::ReleaseTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return;
		updateNormalMapLock.lock();
		updateNormalMapTaskID[taskIDsrc.refrID].erase(taskIDsrc.taskName);
		updateNormalMapLock.unlock();
	}
	std::uint64_t TaskManager::GetCurrentTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return -1;
		std::uint64_t taskID = -1;
		updateNormalMapLock.lock_shared();
		if (auto found1 = updateNormalMapTaskID.find(taskIDsrc.refrID); found1 != updateNormalMapTaskID.end()) {
			if (auto found2 = found1->second.find(taskIDsrc.taskName); found2 != found1->second.end())
				taskID = found2->second;
		}
		updateNormalMapLock.unlock_shared();
		return taskID;
	}
	bool TaskManager::IsValidTaskID(TaskID taskIDsrc) 
	{
		if (taskIDsrc.refrID == 0 || taskIDsrc.taskName.empty())
			return false;
		return GetCurrentTaskID(taskIDsrc) == taskIDsrc.taskID;
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

	EventResult TaskManager::ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*)
	{
		if (!evn || !evn->reference)
			return EventResult::kContinue;
		auto actor = skyrim_cast<RE::Actor*>(evn->reference);
		if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			return EventResult::kContinue;
		QUpdateNormalMap(actor, BipedObjectSlot::kAll);
		return EventResult::kContinue;
	}

	EventResult TaskManager::ProcessEvent(RE::InputEvent* const* evn, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!evn || !*evn)
			return EventResult::kContinue;

		for (RE::InputEvent* input = *evn; input != nullptr; input = input->next)
		{
			if (input->eventType.all(RE::INPUT_EVENT_TYPE::kButton))
			{
				RE::ButtonEvent* button = input->AsButtonEvent();
				if (!button)
					continue;

				using namespace InputManager;
				std::uint32_t keyCode = 0;
				std::uint32_t keyMask = button->idCode;
				if (button->device.all(RE::INPUT_DEVICE::kMouse))
					keyCode = InputMap::kMacro_MouseButtonOffset + keyMask;
				else if (REL::Module::IsVR() &&
						 button->device.underlying() >= INPUT_DEVICE_CROSS_VR::kVirtualKeyboard &&
						 button->device.underlying() <= INPUT_DEVICE_CROSS_VR::kDeviceType_WindowsMRSecondary) {
					keyCode = GetDeviceOffsetForDevice(button->device.underlying()) + keyMask;
				}
				else if (button->device.all(RE::INPUT_DEVICE::kGamepad))
					keyCode = InputMap::GamepadMaskToKeycode(keyMask);
				else
					keyCode = keyMask;

				if (!REL::Module::IsVR())
				{
					if (keyCode >= InputMap::kMaxMacros)
						continue;
				}
				else
				{
					if (keyCode >= InputMap::kMaxMacrosVR)
						continue;
				}

				if (keyCode == Config::GetSingleton().GetHotKey1())
				{
					isPressedHotKey1 = button->IsPressed();
				}
				else if (keyCode == Config::GetSingleton().GetHotKey2())
				{
					if (button->IsUp())
					{
						if (isPressedHotKey1 || Config::GetSingleton().GetHotKey1() == 0)
						{
							RE::Actor* target = nullptr;
							if (auto crossHair = RE::CrosshairPickData::GetSingleton(); crossHair && crossHair->targetActor)
							{
#ifndef ENABLE_SKYRIM_VR
								target = skyrim_cast<RE::Actor*>(crossHair->targetActor.get().get());
#else
								for (std::uint32_t i = 0; i < RE::VRControls::VR_DEVICE::kTotal; i++)
								{
									target = skyrim_cast<RE::Actor*>(crossHair->targetActor[i].get().get());
									if (target)
										break;
								}
#endif
							}
							if (!target)
								target = RE::PlayerCharacter::GetSingleton();
							QUpdateNormalMap(target, BipedObjectSlot::kAll);
						}
					}
					else if (button->HeldDuration() >= 3.0f) //forced reset
					{
						cpuTask = std::make_unique<ThreadPool_TaskModule>(Config::GetSingleton().GetTaskQTick(), Config::GetSingleton().GetDirectTaskQ(), Config::GetSingleton().GetTaskQMax());
						gpuTask = std::make_unique<ThreadPool_TaskModule>(0, Config::GetSingleton().GetDirectTaskQ(), Config::GetSingleton().GetTaskQMax());
					}
				}
			}
		}
		return EventResult::kContinue;
	}
}
