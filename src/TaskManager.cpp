#include "TaskManager.h"

namespace Mus {
	void TaskManager::Init(bool dataLoaded)
	{
		if (!dataLoaded)
		{
			if (const auto Menu = RE::UI::GetSingleton(); Menu)
				Menu->AddEventSink<RE::MenuOpenCloseEvent>(this);
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
		RunBakeQueue();
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
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, bipedSlot]() {
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
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, bipedSlot]() {
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
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, bipedSlot]() {
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

	void TaskManager::RunBakeQueue()
	{
		bakeQueueLock.lock();
		auto bakeQueue_ = bakeSlotQueue;
		bakeSlotQueue.clear();
		bakeQueueLock.unlock();
		concurrency::parallel_for_each(bakeQueue_.begin(), bakeQueue_.end(), [&](auto& queue) {
			RE::Actor* actor = GetFormByID<RE::Actor*>(queue.first);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				return;
			bakeQueueLock.lock_shared();
			for (auto& target : GetGeometries(actor, queue.second))
			{
				bakeGeometryQueue[actor->formID].push_back(target);
			}
			bakeQueueLock.unlock_shared();
		});
		bakeQueueLock.lock();
		auto bakeGeometryQueue_ = bakeGeometryQueue;
		bakeGeometryQueue.clear();
		concurrency::parallel_for_each(bakeGeometryQueue_.begin(), bakeGeometryQueue_.end(), [&](auto& queue) {
			RE::Actor* actor = GetFormByID<RE::Actor*>(queue.first);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				return;
			if (isBaking[actor->formID])
			{
				bakeGeometryQueue[actor->formID] = queue.second;
				return;
			}
			std::unordered_set<RE::BSGeometry*> targets(queue.second.begin(), queue.second.end());
			QUpdateNormalMapImpl(actor, GetAllGeometries(actor), targets);
		});
		bakeQueueLock.unlock();
	}

	void TaskManager::RunDelayTask()
	{
		if (delayTask.empty())
			return;
		
		delayTaskLock.lock();
		auto delayTask_ = delayTask;
		delayTask.clear();
		delayTaskLock.unlock();
		concurrency::parallel_for_each(delayTask_.begin(), delayTask_.end(), [&](auto& task)
		{
			task();
		});
	}
	void TaskManager::RegisterDelayTask(std::function<void()> func) 
	{
		std::lock_guard<std::mutex> lg(delayTaskLock);
		delayTask.push_back(func);
	}
	void TaskManager::RegisterDelayTask(std::uint8_t delayTick, std::function<void()> func) 
	{
		if (delayTick == 0)
			RegisterDelayTask(func);
		else
			RegisterDelayTask([=]() { RegisterDelayTask(delayTick - 1, func); });
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
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
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
			if (auto property = geometry->GetGeometryRuntimeData().properties[State::kProperty].get(); property)
			{
				if (auto alphaProperty = netimmerse_cast<RE::NiAlphaProperty*>(property); alphaProperty)
					return RE::BSVisit::BSVisitControl::kContinue;
			}
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
		auto root = a_actor->loadedData->data3D.get();
		if (!root)
			return geometries;
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (!geo || geo->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (!geo->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect])
				return RE::BSVisit::BSVisitControl::kContinue;
			auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
			auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
			if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
				return RE::BSVisit::BSVisitControl::kContinue;
			if (auto property = geo->GetGeometryRuntimeData().properties[State::kProperty].get(); property)
			{
				if (auto alphaProperty = netimmerse_cast<RE::NiAlphaProperty*>(property); alphaProperty)
					return RE::BSVisit::BSVisitControl::kContinue;
			}
			RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (!material || !material->normalTexture || !material->textureSet)
				return RE::BSVisit::BSVisitControl::kContinue;
			std::string texturePath = GetTexturePath(material->textureSet.get(), RE::BSTextureSet::Texture::kNormal);
			if (texturePath.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (!geo->GetGeometryRuntimeData().skinInstance)
				return RE::BSVisit::BSVisitControl::kContinue;

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
						return RE::BSVisit::BSVisitControl::kContinue;
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
				geometries.emplace(geo);
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		return geometries;
	}

	bool TaskManager::QUpdateNormalMap(RE::Actor* a_actor, std::uint32_t bipedSlot)
	{
		if (!a_actor)
			return false;
		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
			return true;
		if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
			return true;
		if (GetSex(a_actor) == RE::SEX::kMale && !Config::GetSingleton().GetMaleEnable())
			return true;
		if (GetSex(a_actor) == RE::SEX::kFemale && !Config::GetSingleton().GetFemaleEnable())
			return true;
		if (bipedSlot & BipedObjectSlot::kHead && !Config::GetSingleton().GetHeadEnable())
			bipedSlot &= ~BipedObjectSlot::kHead;
		bakeQueueLock.lock_shared();
		bakeSlotQueue[id] |= bipedSlot;
		bakeQueueLock.unlock_shared();
		logger::debug("{}::{:x}::{} : queue added 0x{:x}", __func__, id, actorName, bipedSlot);
		return true;
	}
	bool TaskManager::QUpdateNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_updateTargets)
	{
		if (!a_actor)
			return false;
		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		if (isPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
			return true;
		if (!isPlayer(id) && !Config::GetSingleton().GetNPCEnable())
			return true;
		if (GetSex(a_actor) == RE::SEX::kMale && !Config::GetSingleton().GetMaleEnable())
			return true;
		if (GetSex(a_actor) == RE::SEX::kFemale && !Config::GetSingleton().GetFemaleEnable())
			return true;
		bakeQueueLock.lock_shared();
		for (auto& target : a_updateTargets)
		{
			bakeGeometryQueue[a_actor->formID].push_back(target);
		}
		bakeQueueLock.unlock_shared();
		logger::debug("{}::{:x}::{} : queue added {}", __func__, id, actorName, a_updateTargets.size());
		return true;
	}

	bool TaskManager::QUpdateNormalMapImpl(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::unordered_set<RE::BSGeometry*> a_updateTargets)
	{
		if (!a_actor)
			return false;

		if (a_srcGeometies.empty() || a_updateTargets.empty())
		{
			logger::error("{}::{:x}::{} : no valid geometry found", __func__, a_actor->formID, a_actor->GetName());
			return false;
		}

		auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
		if (!condition.Enable)
			return true;

		logger::debug("{}::{:x}::{} : get geometry datas...", __func__, a_actor->formID, a_actor->GetName());
		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		float detailStrength = condition.DetailStrength;
		if (Papyrus::detailStrengthMap.find(id) != Papyrus::detailStrengthMap.end())
			detailStrength = Papyrus::detailStrengthMap[id];
		auto gender = GetSex(a_actor);
		GeometryData newGeometryData;
		BakeSet newBakeSet;
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
			if (auto property = geo->GetGeometryRuntimeData().properties[State::kProperty].get(); property)
			{
				if (auto alphaProperty = netimmerse_cast<RE::NiAlphaProperty*>(property); alphaProperty)
					continue;
			}
			RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
			if (!material || !material->normalTexture || !material->textureSet)
				continue;
			std::string texturePath = GetTexturePath(material->textureSet.get(), RE::BSTextureSet::Texture::kNormal);
			if (texturePath.empty())
				continue;
			if (!geo->GetGeometryRuntimeData().skinInstance)
				continue;

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

			if ((!condition.HeadEnable || !Config::GetSingleton().GetHeadEnable()) && slot == RE::BIPED_OBJECT::kHead)
				continue;

			newGeometryData.CopyGeometryData(geo);

			if (a_updateTargets.find(geo) != a_updateTargets.end())
			{
				newBakeSet[geo].geometryName = geo->name.c_str();
				newBakeSet[geo].textureName = GetTextureName(a_actor, slot, geo);
				newBakeSet[geo].srcTexturePath = texturePath;
				newBakeSet[geo].detailTexturePath = GetDetailNormalMapPath(texturePath, condition.ProxyDetailTextureFolder);
				newBakeSet[geo].overlayTexturePath = GetOverlayNormalMapPath(texturePath, condition.ProxyOverlayTextureFolder);
				newBakeSet[geo].maskTexturePath = GetMaskNormalMapPath(texturePath, condition.ProxyMaskTextureFolder);
				newBakeSet[geo].detailStrength = detailStrength;
				logger::debug("{:x}::{} : {} - queue added on bake object normalmap", id, actorName, geo->name.c_str());

				auto found = lastNormalMap[id].find(GeometryData::GetVertexCount(geo));
				if (found != lastNormalMap[id].end())
					Shader::TextureLoadManager::CreateSourceTexture(found->second, material->normalTexture);

				ActorVertexHasher::GetSingleton().Register(a_actor, RE::BIPED_OBJECT(slot));

				if (overrideInterfaceAPI->OverrideInterfaceload())
				{
					if (Config::GetSingleton().GetRemoveSkinOverrides())
						overrideInterfaceAPI->RemoveSkinOverride(a_actor, gender == RE::SEX::kFemale, false, 1 << slot, OverrideInterfaceAPI::Key::ShaderTexture, RE::BGSTextureSet::Textures::kNormal);
					if (Config::GetSingleton().GetRemoveNodeOverrides())
						overrideInterfaceAPI->RemoveNodeOverride(a_actor, gender == RE::SEX::kFemale, geo->name, OverrideInterfaceAPI::Key::ShaderTexture, RE::BGSTextureSet::Textures::kNormal);
					logger::info("{:x}::{} : {} - remove skin/node override for normalmap update", id, actorName, geo->name.c_str());
				}
			}
		}
		QUpdateNormalMapImpl(a_actor->formID, actorName, newGeometryData, newBakeSet);
		return true;
	}
	void TaskManager::QUpdateNormalMapImpl(RE::FormID a_actorID, std::string a_actorName, GeometryData& a_geoData, BakeSet& a_bakeSet)
	{
		isBaking[a_actorID] = true;
		actorThreads->submitAsync([this, a_actorID, a_actorName, a_geoData, a_bakeSet]() {
			auto geoData = a_geoData;
			auto bakeSet = a_bakeSet;
			if (!ObjectNormalMapUpdater::GetSingleton().CreateGeometryResourceData(a_actorID, geoData))
			{
				logger::error("{:x}::{} : Failed to get geometry data", a_actorID, a_actorName);
				isBaking[a_actorID] = false;
				return;
			}

			ObjectNormalMapUpdater::BakeResult textures;
			if (Config::GetSingleton().GetGPUEnable())
				textures = ObjectNormalMapUpdater::GetSingleton().UpdateObjectNormalMapGPU(a_actorID, geoData, bakeSet);
			else
				textures = ObjectNormalMapUpdater::GetSingleton().UpdateObjectNormalMap(a_actorID, geoData, bakeSet);
			if (textures.empty())
			{
				logger::error("{:x}::{} : Failed to bake object normalmap", a_actorID, a_actorName);
				isBaking[a_actorID] = false;
				return;
			}

			RegisterDelayTask([this, a_actorID, a_actorName, textures]() {
				auto actor = GetFormByID<RE::Actor*>(a_actorID);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					logger::error("{:x}::{} : invalid reference", a_actorID, a_actorName);
					isBaking[a_actorID] = false;
					return;
				}

				auto root = actor->loadedData->data3D.get();

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
					if (!found->normalmap)
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
					lastNormalMap[a_actorID][found->vertexCount] = found->textureName;
					logger::info("{:x}::{}::{} : bake object normalmap done", a_actorID, a_actorName, geo->name.c_str());
					return RE::BSVisit::BSVisitControl::kContinue;
				});
				isBaking[a_actorID] = false;
			});
		});
	}

	std::string TaskManager::GetDetailNormalMapPath(std::string a_normalMapPath)
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
	std::string TaskManager::GetDetailNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder)
	{
		std::string result = GetDetailNormalMapPath(a_normalMapPath);
		if (!result.empty() || a_proxyFolder.empty())
			return result;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			result = GetDetailNormalMapPath(folder + "\\" + filename + ".dds");
			if (!result.empty())
				return result;
		}
		return "";
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

	std::string TaskManager::GetMaskNormalMapPath(std::string a_normalMapPath)
	{
		constexpr std::string_view prefix = "Textures\\";
		constexpr std::string_view msn_suffix = "_msn";
		constexpr std::string_view n_suffix = "_n";
		constexpr std::string_view m_suffix = "_m";
		if (a_normalMapPath.empty())
			return "";
		if (!stringStartsWith(a_normalMapPath, prefix.data()))
			a_normalMapPath = prefix.data() + a_normalMapPath;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();

		if (stringEndsWith(filename, msn_suffix.data())) //_msn -> _n_m
		{
			std::string filename_n_m = stringRemoveEnds(filename, msn_suffix.data());
			filename_n_m += n_suffix;
			filename_n_m += m_suffix;
			std::string fullPath = (file.parent_path() / (filename_n_m + ".dds")).string();
			if (IsExistFile(fullPath, ExistType::textures))
				return fullPath;
		}
		if (stringEndsWith(filename, n_suffix.data())) //_n -> _n_m
		{
			std::string filename_n_m = filename + m_suffix.data();
			std::string fullPath = (file.parent_path() / (filename_n_m + ".dds")).string();
			if (IsExistFile(fullPath, ExistType::textures))
				return fullPath;
		}
		return "";
	}
	std::string TaskManager::GetMaskNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder)
	{
		std::string result = GetMaskNormalMapPath(a_normalMapPath);
		if (!result.empty() || a_proxyFolder.empty())
			return result;
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			a_normalMapPath = folder + "\\" + filename + ".dds";
			result = GetMaskNormalMapPath(a_normalMapPath);
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
							isResetTasks = false;
						}
					}
					else if (button->HeldDuration() >= 3.0f) //forced reset
					{
						if (isResetTasks)
							return EventResult::kContinue;
						auto coreCount = Mus::Config::GetSingleton().GetPriorityCoreCount();
						std::uint32_t actorThreads = std::max(2.0f, ceil((float)coreCount / 4.0f));
						Mus::actorThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(actorThreads);
						std::uint32_t processingThreads = std::max(2.0f, ceil((float)coreCount / 4.0f * 3.0f));
						Mus::processingThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(processingThreads);
						g_frameEventDispatcher.removeListener(gpuTask.get());
						gpuTask = std::make_unique<ThreadPool_TaskModule>(0, Config::GetSingleton().GetDirectTaskQ(), Config::GetSingleton().GetTaskQMax());
						g_frameEventDispatcher.addListener(gpuTask.get());
						isBaking.clear();
						logger::info("Reset all tasks done");
						isResetTasks = true;
					}
				}
			}
		}
		return EventResult::kContinue;
	}

	EventResult TaskManager::ProcessEvent(const RE::MenuOpenCloseEvent* evn, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!evn || !evn->menuName.c_str())
			return EventResult::kContinue;

		if (evn->opening)
		{
			if (IsSameString(evn->menuName.c_str(), "RaceSex Menu"))
			{
				IsRaceSexMenu.store(true);
			}
		}
		else
		{
			if (IsSameString(evn->menuName.c_str(), "RaceSex Menu"))
			{
				IsRaceSexMenu.store(false);
			}
		}

		return EventResult::kContinue;
	}
}
