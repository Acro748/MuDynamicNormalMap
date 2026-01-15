#include "TaskManager.h"

namespace Mus {
	void TaskManager::Init()
	{
		if (const auto Menu = RE::UI::GetSingleton(); Menu)
			Menu->AddEventSink<RE::MenuOpenCloseEvent>(this);
		if (const auto NiNodeEvent = SKSE::GetNiNodeUpdateEventSource(); NiNodeEvent)
			NiNodeEvent->AddEventSink<SKSE::NiNodeUpdateEvent>(this);
		if (const auto inputManager = RE::BSInputDeviceManager::GetSingleton(); inputManager)
			inputManager->AddEventSink<RE::InputEvent*>(this);
	}

	void TaskManager::onEvent(const FrameEvent& e)
	{
		if (IsSaveLoading.load())
			return;

		RunDelayTask();
		RunUpdateQueue();
		RunManageResource(false);
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
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (IsInvalidActor(actor))
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
			QUpdateNormalMap(actor, BipedObjectSlot::kAll);
		});
	}
	void TaskManager::onEvent(const ActorChangeHeadPartEvent& e)
	{
		if (!e.actor)
			return;

		RE::FormID id = e.actor->formID;
		std::string name = e.actor->GetName();
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (IsInvalidActor(actor))
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
            QUpdateNormalMap(actor, BipedObjectSlot::kAll);
		});
	}
	void TaskManager::onEvent(const ArmorAttachEvent& e)
	{
		if (!e.actor)
			return;
		if (!e.hasAttached)
			return;

		RE::FormID id = e.actor->formID;
		std::string name = e.actor->GetName();
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name, e]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (IsInvalidActor(actor))
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
            QUpdateNormalMap(actor, kAll);
		});
	}
	void TaskManager::onEvent(const PlayerCellChangeEvent& e)
	{
		if (!e.IsChangedInOut)
			return;

		RunManageResource(true);
	}

	void TaskManager::RunUpdateQueue()
	{

		if (Config::GetSingleton().GetQueueTime())
			PerformanceLog(std::string("updateSlotQueue"), false, false);

		if (!isAfterLoading && !isRevertDone)
		{
            if (!SetImmediately(false))
                return;
            isRevertDone = true;
            logger::info("processing in loading screen done");
		}

		auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera)
			return;
		auto cameraPosition = camera->GetRuntimeData2().pos;

		UpdateSlotQueue updateSlotQueue_;
        for (const auto& map : updateSlotQueue)
        {
            if (map.second > 0)
                updateSlotQueue_[map.first] = map.second;
		}
        if (updateSlotQueue_.empty())
            return;

        concurrency::parallel_for_each(updateSlotQueue_.begin(), updateSlotQueue_.end(), [&](auto& map) {
			RE::Actor* actor = GetFormByID<RE::Actor*>(map.first);
			if (IsInvalidActor(actor))
				return;
			bool isInRange = false;
			if (IsPlayer(actor->formID) || Config::GetSingleton().GetUpdateDistance() <= floatPrecision)
			{
				isInRange = true;
			}
			else
			{
				isInRange = cameraPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) <= Config::GetSingleton().GetUpdateDistance();
			}
            if (isActiveActors[actor->formID] != isInRange)
			{
				if (!isInRange)
				{
					if (!Config::GetSingleton().GetUpdateDistanceVramSave())
						return;
					lastNormalMapLock.lock_shared();
                    bool isFoundLastNormalMap = lastNormalMap.find(actor->formID) != lastNormalMap.end();
					lastNormalMapLock.unlock_shared();
					if (isFoundLastNormalMap)
					{
						if (!isUpdating[actor->formID])
							RemoveNormalMap(actor);
						else
							return;
                    }
				}
				else
				{
					QUpdateNormalMap(actor, BipedObjectSlot::kAll);
				}
			}
			if (isInRange && !isUpdating[actor->formID])
			{
				QUpdateNormalMapImpl(actor, GetAllGeometries(actor), updateSlotQueue[actor->formID]);
				updateSlotQueue[actor->formID] = 0;
            }
            isActiveActors[actor->formID] = isInRange;
		});

        if (isAfterLoading)
        {
            isAfterLoading = false;
            if (SetImmediately(false))
            {
                isRevertDone = true;
                logger::info("processing in loading screen done");
            }
        }

		if (Config::GetSingleton().GetQueueTime())
			PerformanceLog(std::string("updateSlotQueue"), true, false, updateSlotQueue.size());
	}

	void TaskManager::RunDelayTask()
	{
		if (delayTask.empty())
			return;

		if (Config::GetSingleton().GetQueueTime())
			PerformanceLog(std::string(__func__), false, false);

		delayTaskLock.lock();
		auto delayTask_ = delayTask;
		delayTask.clear();
		delayTaskLock.unlock();
		concurrency::parallel_for_each(delayTask_.begin(), delayTask_.end(), [&](auto& task)
		{
			task();
		});

		if (Config::GetSingleton().GetQueueTime())
			PerformanceLog(std::string(__func__), true, false, delayTask_.size());
	}
	void TaskManager::RegisterDelayTask(std::function<void()> func) 
	{
		std::lock_guard<std::mutex> lg(delayTaskLock);
		delayTask.push_back(func);
	}
	void TaskManager::RegisterDelayTask(std::int16_t delayTick, std::function<void()> func) 
	{
		if (delayTick <= 0)
			RegisterDelayTask(func);
		else
			RegisterDelayTask([=]() { RegisterDelayTask(delayTick - 1, func); });
	}

	void TaskManager::RunManageResource(bool isImminently)
	{
		static std::clock_t lastTickTime = currentTime;
		if (lastTickTime + 1000 > currentTime && !isImminently) //1sec
			return;
		lastTickTime = currentTime;
		ReleaseResourceOnUnloadActors();
	}

	std::unordered_set<RE::BSGeometry*> TaskManager::GetAllGeometries(RE::Actor* a_actor)
	{
		std::unordered_set<RE::BSGeometry*> geometries;
		if (IsInvalidActor(a_actor))
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
	bool TaskManager::QUpdateNormalMap(RE::Actor* a_actor, bSlotbit bipedSlot)
	{
		if (!a_actor || bipedSlot == 0)
			return false;
		RE::FormID id = a_actor->formID;
		std::string actorName = a_actor->GetName();
		if (IsPlayer(id) && !Config::GetSingleton().GetPlayerEnable())
			return true;
		if (!IsPlayer(id) && !Config::GetSingleton().GetNPCEnable())
			return true;
		if (GetSex(a_actor) == RE::SEX::kMale && !Config::GetSingleton().GetMaleEnable())
			return true;
		if (GetSex(a_actor) == RE::SEX::kFemale && !Config::GetSingleton().GetFemaleEnable())
			return true;
		if (bipedSlot & BipedObjectSlot::kHead && !Config::GetSingleton().GetHeadEnable())
			bipedSlot &= ~BipedObjectSlot::kHead;
		updateSlotQueue[id] |= bipedSlot;
		if (isActiveActors.find(id) == isActiveActors.end())
			isActiveActors[id] = false;
		logger::debug("{}::{:x}::{} : queue added 0x{:x}", __func__, id, actorName, bipedSlot);
		return true;
	}

	bool TaskManager::QUpdateNormalMapImpl(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, bSlotbit bipedSlot)
	{
		if (!a_actor || bipedSlot == 0)
			return false;

		if (a_srcGeometies.empty())
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
		GeometryDataPtr newGeometryData = std::make_shared<GeometryData>();
		UpdateSet newUpdateSet;
		for (auto& geo : a_srcGeometies)
		{
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (!geo || geo->name.empty())
				continue;
			if (auto extraData = geo->GetExtraData<RE::NiIntegerExtraData>(NoDynamicNormalMapExtraDataName); extraData && extraData->value > 0)
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
			if (!material || !material->normalTexture || material->normalTexture->name.empty())
				continue;
			std::string texturePath = GetOriginalTexturePath(lowLetter(material->normalTexture->name.c_str()));
			if (texturePath.empty() || !IsExistFile(texturePath, ExistType::textures))
			{
				if (material->textureSet)
				{
					texturePath = GetTexturePath(material->textureSet.get(), RE::BGSTextureSet::Textures::kNormal);
					if (texturePath.empty() || !IsExistFile(texturePath, ExistType::textures))
						continue;
				}
			}
			if (!geo->GetGeometryRuntimeData().skinInstance)
				continue;

			bSlot slot = nif::GetBipedSlot(geo, condition.DynamicTriShapeAsHead);
			if ((!condition.HeadEnable || !Config::GetSingleton().GetHeadEnable()) && slot == RE::BIPED_OBJECT::kHead)
				continue;

			newGeometryData->CopyGeometryData(geo);

			if (bipedSlot & 1 << slot)
			{
				newUpdateSet[geo].slot = slot;
				newUpdateSet[geo].geometryName = geo->name.c_str();
				newUpdateSet[geo].textureName = GetTextureName(a_actor, slot, texturePath);
				newUpdateSet[geo].srcTexturePath = texturePath;

				std::string saveDetailTexturePath = GetDetailNormalMapPath(a_actor);
				newUpdateSet[geo].detailTexturePath = saveDetailTexturePath.empty() ? GetDetailNormalMapPath(texturePath, condition.ProxyDetailTextureFolder, condition.ProxyFirstScan) : saveDetailTexturePath;

				std::string saveOverlayTexturePath = GetOverlayNormalMapPath(a_actor);
				newUpdateSet[geo].overlayTexturePath = saveOverlayTexturePath.empty() ? GetOverlayNormalMapPath(texturePath, condition.ProxyOverlayTextureFolder, condition.ProxyFirstScan) : saveOverlayTexturePath;

				std::string saveMaskTexturePath = GetMaskNormalMapPath(a_actor);
				newUpdateSet[geo].maskTexturePath = saveMaskTexturePath.empty() ? GetMaskNormalMapPath(texturePath, condition.ProxyMaskTextureFolder, condition.ProxyFirstScan) : saveMaskTexturePath;

				newUpdateSet[geo].detailStrength = detailStrength;

				logger::debug("{:x}::{} : {} - queue added on update object normalmap", id, actorName, geo->name.c_str());

				if (!Config::GetSingleton().GetRevertNormalMap())
                {
                    bool isFound = false;
                    lastNormalMapLock.lock_shared();
                    if (auto found = lastNormalMap.find(id); found != lastNormalMap.end())
                        isFound = found->second.find({newUpdateSet[geo].slot, newUpdateSet[geo].textureName}) != found->second.end();
					lastNormalMapLock.unlock_shared();
                    if (isFound)
                        Shader::TextureLoadManager::CreateSourceTexture(newUpdateSet[geo].textureName, material->normalTexture);
				}

				ActorVertexHasher::GetSingleton().Register(a_actor, geo);

				if (overrideInterfaceAPI->OverrideInterfaceload())
				{
					if (Config::GetSingleton().GetRemoveSkinOverrides())
					{
						overrideInterfaceAPI->RemoveSkinOverride(a_actor, gender == RE::SEX::kFemale, false, 1 << slot, OverrideInterfaceAPI::Key::ShaderTexture, RE::BGSTextureSet::Textures::kNormal);
						logger::info("{:x}::{} : {} - remove skin override for normalmap update", id, actorName, geo->name.c_str());
					}
					if (Config::GetSingleton().GetRemoveNodeOverrides())
					{
						overrideInterfaceAPI->RemoveNodeOverride(a_actor, gender == RE::SEX::kFemale, geo->name, OverrideInterfaceAPI::Key::ShaderTexture, RE::BGSTextureSet::Textures::kNormal);
						logger::info("{:x}::{} : {} - remove node override for normalmap update", id, actorName, geo->name.c_str());
					}
				}
			}
		}
		QUpdateNormalMapImpl(a_actor->formID, actorName, newGeometryData, newUpdateSet);
		return true;
	}
	void TaskManager::QUpdateNormalMapImpl(RE::FormID a_actorID, std::string a_actorName, GeometryDataPtr a_geoData, UpdateSet a_updateSet)
	{
		if (!a_geoData || a_updateSet.empty() || isUpdating[a_actorID])
			return;

		isUpdating[a_actorID] = true;
        auto func = [this, a_actorID, a_actorName, a_geoData, a_updateSet]() {
            if (Config::GetSingleton().GetFullUpdateTime())
                PerformanceLog(std::string("QUpdateNormalMapImpl") + "::" + SetHex(a_actorID, 0), false, false);

            if (!ObjectNormalMapUpdater::GetSingleton().CreateGeometryResourceData(a_actorID, a_geoData))
            {
                logger::error("{:x}::{} : Failed to get geometry data", a_actorID, a_actorName);
                isUpdating[a_actorID] = false;
                return;
            }

            ObjectNormalMapUpdater::UpdateResult textures;
            if (Config::GetSingleton().GetGPUEnable())
                textures = ObjectNormalMapUpdater::GetSingleton().UpdateObjectNormalMapGPU(a_actorID, a_geoData, a_updateSet);
            else
                textures = ObjectNormalMapUpdater::GetSingleton().UpdateObjectNormalMap(a_actorID, a_geoData, a_updateSet);
            if (textures.empty())
            {
                logger::error("{:x}::{} : Failed to update object normalmap", a_actorID, a_actorName);
                isUpdating[a_actorID] = false;
                return;
            }

            RegisterDelayTask([this, a_actorID, a_actorName, textures]() {
                auto actor = GetFormByID<RE::Actor*>(a_actorID);
                if (IsInvalidActor(actor))
                {
                    logger::error("{:x}::{} : invalid reference", a_actorID, a_actorName);
                    isUpdating[a_actorID] = false;
                    return;
                }

                auto root = actor->loadedData->data3D.get();
                std::unordered_map<std::string, RE::NiSourceTexturePtr> createdTextures;
                RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
                    using State = RE::BSGeometry::States;
                    using Feature = RE::BSShaderMaterial::Feature;
                    if (!geo || geo->name.empty())
                        return RE::BSVisit::BSVisitControl::kContinue;
                    if (auto extraData = geo->GetExtraData<RE::NiIntegerExtraData>(NoDynamicNormalMapExtraDataName); extraData && extraData->value == 2)
                        return RE::BSVisit::BSVisitControl::kContinue;

                    auto found = textures.end();
                    if (IsContainString(geo->name.c_str(), "[Ovl") || IsContainString(geo->name.c_str(), "[SOvl") || IsContainString(geo->name.c_str(), "overlay"))
                    {
                        if (!Config::GetSingleton().GetApplyOverlay())
                            return RE::BSVisit::BSVisitControl::kContinue;

                        bSlot slot = 0;
                        if (IsContainString(geo->name.c_str(), "Body"))
                            slot = RE::BIPED_OBJECT::kBody;
                        else if (IsContainString(geo->name.c_str(), "Hands"))
                            slot = RE::BIPED_OBJECT::kHands;
                        else if (IsContainString(geo->name.c_str(), "Feet"))
                            slot = RE::BIPED_OBJECT::kFeet;
                        else if (IsContainString(geo->name.c_str(), "overlay") || IsContainString(geo->name.c_str(), "Face"))
                            slot = RE::BIPED_OBJECT::kHead;
                        else
                            return RE::BSVisit::BSVisitControl::kContinue;

                        found = std::find_if(textures.begin(), textures.end(), [&](ObjectNormalMapUpdater::NormalMapResult normalmap) {
                            return normalmap.slot == slot &&
                                   !IsContainString(normalmap.geoName, "Vagina") && !IsContainString(normalmap.geoName, "Anus") && !IsContainString(normalmap.geoName, "Canal");
                        });
                    }
                    else
                    {
                        found = std::find_if(textures.begin(), textures.end(), [&](ObjectNormalMapUpdater::NormalMapResult normalmap) {
                            return normalmap.geometry == geo;
                        });
                    }

                    auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
                    if (!effect)
                        return RE::BSVisit::BSVisitControl::kContinue;
                    auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
                    if (!lightingShader || !lightingShader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
                        return RE::BSVisit::BSVisitControl::kContinue;
                    RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
                    if (!material || !material->normalTexture)
                        return RE::BSVisit::BSVisitControl::kContinue;

                    if (found == textures.end())
                    {
                        auto& skinInstance = geo->GetGeometryRuntimeData().skinInstance;
                        if (!skinInstance)
                            return RE::BSVisit::BSVisitControl::kContinue;

                        auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance.get());
                        if (dismember)
                        {
                            std::string texturePath = GetOriginalTexturePath(lowLetter(material->normalTexture->name.c_str()));
                            found = std::find_if(textures.begin(), textures.end(), [&](ObjectNormalMapUpdater::NormalMapResult normalmap) {
                                for (std::int32_t p = 0; p < dismember->GetRuntimeData().numPartitions; p++)
                                {
                                    bSlot slot;
                                    auto pslot = dismember->GetRuntimeData().partitions[p].slot;
                                    if (pslot < 30 || pslot >= RE::BIPED_OBJECT::kEditorTotal + 30)
                                    {
                                        if (geo->AsDynamicTriShape())
                                        { // maybe head
                                            slot = RE::BIPED_OBJECT::kHead;
                                        }
                                        else if (pslot == 0) // BP_TORSO
                                        {
                                            slot = RE::BIPED_OBJECT::kBody;
                                        }
                                        else // unknown slot
                                            continue;
                                    }
                                    else
                                        slot = pslot - 30;
                                    if (normalmap.slot == slot && IsSameString(normalmap.texturePath, texturePath) && !IsSameString(normalmap.geoName, geo->name.c_str()))
                                        return true;
                                }
                                return false;
                            });
                        }
                        if (found == textures.end())
                            return RE::BSVisit::BSVisitControl::kContinue;
                    }
                    if (!found->texture || !found->texture->normalmapTexture2D || !found->texture->normalmapShaderResourceView)
                        return RE::BSVisit::BSVisitControl::kContinue;

                    RE::NiSourceTexturePtr normalmap = nullptr;
                    if (auto texIt = createdTextures.find(found->textureName); texIt != createdTextures.end())
                    {
                        normalmap = texIt->second;
                    }
                    else
                    {
                        if (Shader::TextureLoadManager::GetSingleton().CreateNiTexture(found->textureName, found->texture->normalmapTexture2D, found->texture->normalmapShaderResourceView, normalmap) < 0)
                        {
                            logger::error("{:x}::{}::{} : Failed to create NiTexture", a_actorID, a_actorName, geo->name.c_str());
                            return RE::BSVisit::BSVisitControl::kContinue;
                        }
                        createdTextures.insert(std::make_pair(found->textureName, normalmap));
                        if (Config::GetSingleton().GetLogLevel() <= spdlog::level::level_enum::debug)
                        {
                            TextureLog(found->texture->normalmapTexture2D.Get());
                            TextureLog(found->texture->normalmapShaderResourceView.Get());
                        }
                    }

                    if (Config::GetSingleton().GetDebugTexture())
                        material->diffuseTexture = normalmap;
                    material->normalTexture = normalmap;

                    lastNormalMapLock.lock();
                    lastNormalMap[a_actorID].insert({found->slot, found->textureName});
                    lastNormalMapLock.unlock();
                    logger::info("{:x}::{}::{} : update object normalmap done", a_actorID, a_actorName, geo->name.c_str());
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
                isUpdating[a_actorID] = false;
            });

            if (Config::GetSingleton().GetFullUpdateTime())
                PerformanceLog(std::string("QUpdateNormalMapImpl") + "::" + SetHex(a_actorID, 0), true, false);
        };
        if (isImmediately)
        {
            currentActorThreads.load()->submitAsync(func).get();
        }
        else
            currentActorThreads.load()->submitAsync(func);
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
	std::string TaskManager::GetDetailNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan)
	{
		std::string result;
		if (!a_proxyFirstScan)
		{
			result = GetDetailNormalMapPath(a_normalMapPath);
			if (!result.empty() || a_proxyFolder.empty())
				return result;
		}
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			result = GetDetailNormalMapPath(folder + "\\" + filename + ".dds");
			if (!result.empty())
				return result;
		}
		if (a_proxyFirstScan)
		{
			result = GetDetailNormalMapPath(a_normalMapPath);
			if (!result.empty())
				return result;
		}
		return "";
	}
	std::string TaskManager::GetDetailNormalMapPath(RE::Actor* a_actor)
	{
		if (!a_actor)
			return "";
		auto found = Papyrus::normalmaps[Papyrus::normalmapTypes::detail].find(a_actor->formID);
		if (found == Papyrus::normalmaps[Papyrus::normalmapTypes::detail].end())
			return "";
		return found->second;
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
	std::string TaskManager::GetOverlayNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan)
	{
		std::string result;
		if (!a_proxyFirstScan)
		{
			result = GetOverlayNormalMapPath(a_normalMapPath);
			if (!result.empty() || a_proxyFolder.empty())
				return result;
		}
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			a_normalMapPath = folder + "\\" + filename + ".dds";
			result = GetOverlayNormalMapPath(a_normalMapPath);
			if (!result.empty())
				return result;
		}
		if (a_proxyFirstScan)
		{
			result = GetOverlayNormalMapPath(a_normalMapPath);
			if (!result.empty())
				return result;
		}
		return "";
	}
	std::string TaskManager::GetOverlayNormalMapPath(RE::Actor* a_actor)
	{
		if (!a_actor)
			return "";
		auto found = Papyrus::normalmaps[Papyrus::normalmapTypes::overlay].find(a_actor->formID);
		if (found == Papyrus::normalmaps[Papyrus::normalmapTypes::overlay].end())
			return "";
		return found->second;
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
	std::string TaskManager::GetMaskNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan)
	{
		std::string result;
		if (!a_proxyFirstScan)
		{
			result = GetMaskNormalMapPath(a_normalMapPath);
			if (!result.empty() || a_proxyFolder.empty())
				return result;
		}
		std::filesystem::path file(a_normalMapPath);
		std::string filename = file.stem().string();
		for (auto& folder : a_proxyFolder)
		{
			a_normalMapPath = folder + "\\" + filename + ".dds";
			result = GetMaskNormalMapPath(a_normalMapPath);
			if (!result.empty())
				return result;
		}
		if (a_proxyFirstScan)
		{
			result = GetMaskNormalMapPath(a_normalMapPath);
			if (!result.empty())
				return result;
		}
		return "";
	}
	std::string TaskManager::GetMaskNormalMapPath(RE::Actor* a_actor)
	{
		if (!a_actor)
			return "";
		auto found = Papyrus::normalmaps[Papyrus::normalmapTypes::mask].find(a_actor->formID);
		if (found == Papyrus::normalmaps[Papyrus::normalmapTypes::mask].end())
			return "";
		return found->second;
	}

	std::string TaskManager::FixTexturePath(std::string texturePath)
	{
		texturePath = FixPath(texturePath);
		texturePath = stringRemoveStarts(texturePath, "data\\");
		if (!stringStartsWith(texturePath, "textures\\"))
			texturePath = "textures\\" + texturePath;
		return texturePath;
	}

	std::string TaskManager::GetTextureName(RE::Actor* a_actor, bSlot a_bipedSlot, std::string a_texturePath)
	{ // ActorID + BipedSlot + TexturePath
		if (!a_actor || a_texturePath.empty())
			return "EMPTY";
		a_texturePath = stringRemoveStarts(a_texturePath, "Data\\");
		a_texturePath = stringRemoveStarts(a_texturePath, "Textures\\");
		return MDNMPrefix + GetHexStr(a_actor->formID) + "::" + std::to_string(a_bipedSlot) + "::" + a_texturePath;
	}
	bool TaskManager::GetTextureInfo(std::string a_textureName, TextureInfo& a_textureInfo)
	{ // ActorID + BipedSlot + TexturePath
		if (a_textureName.empty() || !IsCreatedByMDNM(a_textureName))
			return false;
		a_textureName = stringRemoveStarts(a_textureName, MDNMPrefix);
		auto frag = split(a_textureName, "::");
		if (frag.size() < 3)
			return false;
		a_textureInfo.actorID = GetHex(frag[0]);
		a_textureInfo.bipedSlot = Config::GetUIntValue(frag[1]);
		a_textureInfo.texturePath = frag[2];
		return true;
	}
	std::string TaskManager::GetOriginalTexturePath(std::string a_textureName)
	{
		TextureInfo info;
		if (GetTextureInfo(a_textureName, info))
			return info.texturePath;
		return FixTexturePath(a_textureName);
	}
	bool TaskManager::IsCreatedByMDNM(std::string a_textureName)
	{
		return stringStartsWith(a_textureName, MDNMPrefix);
	}

	bool TaskManager::RemoveNormalMap(RE::Actor* a_actor)
	{
		if (IsInvalidActor(a_actor))
			return false;

		for(auto& geo : GetAllGeometries(a_actor))
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
			std::string texturePath = lowLetter(GetTexturePath(material->textureSet.get(), RE::BSTextureSet::Texture::kNormal));
			if (texturePath.empty())
				continue;
			RE::NiPointer<RE::NiSourceTexture> texture;
			Shader::TextureLoadManager::GetSingleton().LoadTexture(texturePath.c_str(), 1, texture, false);
			if (!texture)
				continue;
			material->normalTexture = texture;
		}

        SlotTexSet textures;
		lastNormalMapLock.lock();
        auto found = lastNormalMap.find(a_actor->formID);
        if (found != lastNormalMap.end())
        {
            textures = found->second;
            lastNormalMap.erase(a_actor->formID);
        }
		lastNormalMapLock.unlock();
        ReleaseNormalMap(textures);
		return true;
	}

	EventResult TaskManager::ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*)
	{
		if (!evn || !evn->reference)
			return EventResult::kContinue;
		auto actor = skyrim_cast<RE::Actor*>(evn->reference);
		if (!actor)
			return EventResult::kContinue; 
		RE::FormID id = actor->formID;
		std::string name = actor->GetName();
		RegisterDelayTask(Config::GetSingleton().GetUpdateDelayTick(), [this, id, name]() {
			RE::Actor* actor = GetFormByID<RE::Actor*>(id);
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				logger::error("{:x} {} : invalid reference. so skip", id, name);
				return;
			}
			QUpdateNormalMap(actor, BipedObjectSlot::kAll);
		});
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
                        isResetTasks = false;
                        if (button->HeldDuration() < 3.0f && (isPressedHotKey1 || Config::GetSingleton().GetHotKey1() == 0))
						{
							RE::Actor* target = nullptr;
							if (auto consoleRef = RE::Console::GetSelectedRef(); consoleRef && Config::GetSingleton().GetUseConsoleRef())
							{
								target = skyrim_cast<RE::Actor*>(consoleRef.get());
							}
							else if (auto crossHair = RE::CrosshairPickData::GetSingleton(); crossHair && crossHair->targetActor)
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
							std::string notification = "MDNM : Re-update ";
							notification += target->GetName();
							notification += " " + GetHexStr(target->formID);
							RE::DebugNotification(notification.c_str());
						}
					}
					else if (button->HeldDuration() >= 3.0f) //forced reset
					{
						if (isResetTasks)
							return EventResult::kContinue;
                        if (ObjectNormalMapUpdater::GetSingleton().GetCount() > 0)
                        {
                            RE::DebugNotification("MDNM : Some normalmaps are still being updated");
                            logger::warn("Some normalmaps are still being updated...");
                            return EventResult::kContinue;
                        }
						g_frameEventDispatcher.removeListener(gpuTask.get());
						g_armorAttachEventEventDispatcher.removeListener(&Mus::ActorVertexHasher::GetSingleton());
						g_facegenNiNodeEventDispatcher.removeListener(&Mus::ActorVertexHasher::GetSingleton());
						g_actorChangeHeadPartEventDispatcher.removeListener(&ActorVertexHasher::GetSingleton());
						if (const auto NiNodeEvent = SKSE::GetNiNodeUpdateEventSource(); NiNodeEvent)
							NiNodeEvent->RemoveEventSink(&Mus::ActorVertexHasher::GetSingleton());
						Mus::Config::GetSingleton().LoadConfig();
						InitialSetting();
						isUpdating.clear();
						ConditionManager::GetSingleton().InitialConditionList();
						static_cast<MultipleConfig*>(&Config::GetSingleton())->LoadConditionFile();
						ConditionManager::GetSingleton().SortConditions();
						ObjectNormalMapUpdater::GetSingleton().Init();
						RE::DebugNotification("MDNM : Reload done");
						logger::info("Reload done");
						isResetTasks = true;
					}
				}
				else if (keyCode == 29) //ctrl
				{
					if (button->IsDown())
					{
						isPressedExportHotkey1 = true;
					}
					else if (button->IsUp())
					{
						isPressedExportHotkey1 = false;
					}
				}
				else if (keyCode == 88) //F12
				{
					if (button->IsUp() && isPressedExportHotkey1)
					{
						if (Config::GetSingleton().GetLogLevel() >= 2)
							continue;

						logger::info("Print textures...");
						RE::Actor* target = nullptr;
						if (auto consoleRef = RE::Console::GetSelectedRef(); consoleRef && Config::GetSingleton().GetUseConsoleRef())
						{
							target = skyrim_cast<RE::Actor*>(consoleRef.get());
						}
						if (auto crossHair = RE::CrosshairPickData::GetSingleton(); !target && crossHair && crossHair->targetActor)
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

						SlotTexSet textures;
						lastNormalMapLock.lock_shared();
                        if (auto found = lastNormalMap.find(target->formID); found != lastNormalMap.end())
                            textures = found->second;
						lastNormalMapLock.unlock_shared();

						for (auto& pair : textures)
						{
							auto texture = Shader::TextureLoadManager::GetSingleton().GetNiTexture(pair.textureName);
							if (!texture)
								continue;

							TextureInfo info;
							if (!GetTextureInfo(pair.textureName, info))
								continue;

							auto texturePath = stringRemoveEnds(info.texturePath, ".dds");
							texturePath = texturePath + "_" + SetHex(info.actorID, false) + "_" + std::to_string(info.bipedSlot) + ".dds";;

							if (!stringStartsWith(texturePath, "Textures\\"))
								texturePath = GetRuntimeTexturesDirectory() + texturePath;
							else if (stringStartsWith(texturePath, "Data\\"))
								texturePath = GetRuntimeDataDirectory() + texturePath;

							if (Shader::TextureLoadManager::GetSingleton().PrintTexture(texturePath, texture.Get()))
							{
								logger::info("Print texture done : {}", texturePath);
							}
						}
						RE::DebugNotification("MDNM : Print texture done");
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
                logger::info("RaceSexMenu Open");
				IsRaceSexMenu.store(true);
			}
			else if (IsSameString(evn->menuName.c_str(), "Loading Menu"))
            {
                //logger::info("Loading Menu Open");
                if (SetImmediately(true))
                {
                    isAfterLoading = true;
                    isRevertDone = false;
                }
			}
		}
		else
		{
			if (IsSameString(evn->menuName.c_str(), "RaceSex Menu"))
            {
                logger::info("RaceSexMenu Close");
				IsRaceSexMenu.store(false);
            }
            else if (IsSameString(evn->menuName.c_str(), "Loading Menu"))
            {
                //logger::info("Loading Menu Close");
            }
        }
        return EventResult::kContinue;
	}


	void TaskManager::ReleaseResourceOnUnloadActors()
	{
		memoryManageThreads->submitAsync([&]() {
            lastNormalMapLock.lock_shared();
            auto lastNormalMap_ = lastNormalMap;
            lastNormalMapLock.unlock_shared();
            if (lastNormalMap_.empty())
                return;
            std::size_t count = lastNormalMap_.size();
            for (auto& map : lastNormalMap_)
			{
				RE::Actor* actor = GetFormByID<RE::Actor*>(map.first);
				if (IsInvalidActor(actor))
				{
					if (isUpdating[map.first])
					{
						continue;
					}
					ReleaseNormalMap(map.second);
					lastNormalMapLock.lock();
                    lastNormalMap.erase(map.first);
					lastNormalMapLock.unlock();
                    count--;
				}
			}
			if (count > 0)
				logger::debug("Current remain NiTexture {}", count);
		});
	}
	void TaskManager::ReleaseNormalMap(RE::FormID a_actorID, bSlot a_slot)
	{
        SlotTexSet textures;
		lastNormalMapLock.lock_shared();
        if (auto found = lastNormalMap.find(a_actorID); found != lastNormalMap.end())
            textures = found->second;
		lastNormalMapLock.unlock_shared();
        for (auto& texture : textures)
		{
			if (texture.slot == a_slot && !texture.textureName.empty())
			{
				Shader::TextureLoadManager::GetSingleton().ReleaseNiTexture(texture.textureName);
				logger::debug("{} : Removed unused NiTexture", texture.textureName);
				lastNormalMapLock.lock();
				lastNormalMap[a_actorID].erase(texture);
				lastNormalMapLock.unlock();
			}
		}
	}
    void TaskManager::ReleaseNormalMap(SlotTexSet& textures)
	{
        for (auto& texture : textures)
		{
			if (!texture.textureName.empty())
			{
				Shader::TextureLoadManager::GetSingleton().ReleaseNiTexture(texture.textureName);
				logger::debug("{} : Removed unused NiTexture", texture.textureName);
			}
		}
	}
}
