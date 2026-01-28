#include "ActorVertexHasher.h"

namespace Mus {
	void ActorVertexHasher::Init()
	{

	}

	void ActorVertexHasher::onEvent(const FrameEvent& e)
	{
		static std::clock_t lastTickTime = currentTime;

		if (IsSaveLoading.load())
			return;
		if (IsGamePaused.load() && !IsRaceSexMenu.load())
			return;
		if (isDetecting.load())
			return;
		if (lastTickTime + Config::GetSingleton().GetDetectTickMS() > currentTime)
			return;
		CheckingActorHash();
		lastTickTime = currentTime;
	}

	void ActorVertexHasher::onEvent(const FacegenNiNodeEvent& e)
	{
		if (!e.facegenNiNode)
			return;
		RE::Actor* actor = skyrim_cast<RE::Actor*>(e.facegenNiNode->GetUserData());
		if (!actor)
			return;
		SetBlocked(actor->formID, true);
	}

	void ActorVertexHasher::onEvent(const ActorChangeHeadPartEvent& e)
	{
		if (!e.actor)
			return;
		SetBlocked(e.actor->formID, true);
	}

	void ActorVertexHasher::onEvent(const ArmorAttachEvent& e)
	{
		if (e.hasAttached)
			return;
		if (!e.actor)
			return;
		SetBlocked(e.actor->formID, true);
	}

	EventResult ActorVertexHasher::ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*)
	{
		if (!evn || !evn->reference)
			return EventResult::kContinue;
		SetBlocked(evn->reference->formID, true);
		return EventResult::kContinue;
	}

	bool ActorVertexHasher::Register(RE::Actor* a_actor, RE::BSGeometry* a_geo)
	{
		if (!Config::GetSingleton().GetRealtimeDetect())
			return true;
		if (!a_geo || a_geo->name.empty())
			return false;
		if (IsInvalidActor(a_actor))
			return false;
        std::lock_guard lg(actorHashLock);
        auto actorIt = actorHash.find(a_actor->formID);
        if (actorIt == actorHash.end())
		{
			auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
            actorHash[a_actor->formID].IsDynamicTriShapeAsHead = condition.DynamicTriShapeAsHead;
            actorIt = actorHash.find(a_actor->formID);
            if (actorIt == actorHash.end())
                return false;
		}
        auto hashIt = actorIt->second.hash.find(a_geo);
        if (hashIt != actorIt->second.hash.end())
        {
            hashIt->second->SetCheckTexture(false);
            return hashIt->second->Update(a_geo);
		}
		else
		{
            auto newHash = std::make_shared<Hash>(a_geo);
            newHash->SetCheckTexture(false);
            newHash->Update(a_geo);
            actorIt->second.hash.insert(std::make_pair(a_geo, newHash));
            logger::debug("{:x} {} {}: registered for ActorVertexHasher", a_actor->formID, a_actor->GetName(), a_geo->name.c_str());
		}
    }
	bool ActorVertexHasher::RegisterCheckTexture(RE::Actor* a_actor, RE::BSGeometry* a_geo)
	{
        if (!Config::GetSingleton().GetDetectTextureChange())
            return true;
        std::lock_guard lg(actorHashLock);
        auto actorIt = actorHash.find(a_actor->formID);
        if (actorIt == actorHash.end())
            return false;
        auto hashIt = actorIt->second.hash.find(a_geo);
        if (hashIt == actorIt->second.hash.end())
            return false;
        hashIt->second->SetCheckTexture(true);
        return true;
	}

	void ActorVertexHasher::CheckingActorHash()
	{
		if (actorHash.size() == 0)
			return;
		auto camera = RE::PlayerCamera::GetSingleton();
		if (!camera)
			return;

		const std::string funcName = __func__;
		auto cameraPosition = camera->GetRuntimeData2().pos;

		auto actorHashFunc = [&, funcName, cameraPosition](const RE::FormID& id, GeometryHashData& data) {
			RE::NiPointer<RE::Actor> actor = RE::NiPointer(GetFormByID<RE::Actor*>(id));
            if (IsInvalidActor(actor.get()))
			{
				return false;
			}
			if (Config::GetSingleton().GetActorVertexHasherTime2())
				PerformanceLog(funcName + "::" + GetHexStr(id), false, true);

			if (IsBlocked(actor->formID))
				return true;
			if (!IsPlayer(actor->formID) && (Config::GetSingleton().GetDetectDistance() > floatPrecision && cameraPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) > Config::GetSingleton().GetDetectDistance()))
                return true;
            if (!GetHash(actor.get(), data.hash))
                return true;

			bSlotbit bipedSlot = 0;
            for (auto& hash : data.hash)
            {
                bSlot slot = nif::GetBipedSlot(hash.first, data.IsDynamicTriShapeAsHead);
				if (hash.second->GetHash() != hash.second->GetOldHash())
				{
					bipedSlot |= 1 << slot;
					logger::trace("{:x} {} {} : found different hash {:x}", actor->formID, actor->GetName(), slot, hash.second->GetHash());
				}
				else if (hash.second->IsChangedTexture())
				{
                    bipedSlot = TaskManager::BipedObjectSlot::kAll;
                    logger::trace("{:x} {} {} : found different texture", actor->formID, actor->GetName(), slot);
				}
			}
			if (bipedSlot > 0)
			{
				logger::debug("{:x} {} : detected changed slot {:x}", actor->formID, actor->GetName(), bipedSlot);
                TaskManager::GetSingleton().QUpdateNormalMap(actor.get());
			}

			if (Config::GetSingleton().GetActorVertexHasherTime2())
				PerformanceLog(funcName + "::" + GetHexStr(id), true, true);
            return true;
		};

		if (Config::GetSingleton().GetRealtimeDetectOnBackGround())
		{
			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(funcName, false, true);

			{
                std::lock_guard lg(blockActorsLock);
                blockActors.clear();
            }

			backGroundHasherThreads->submitAsync([&, actorHashFunc, funcName]() {
				isDetecting.store(true);
				{
                    std::lock_guard lg(actorHashLock);
                    for (auto it = actorHash.begin(); it != actorHash.end();)
                    {
						if (!actorHashFunc(it->first, it->second))
						{
                            it = actorHash.erase(it);
						}
						else
                        {
                            it++;
						}
                    }
                    if (Config::GetSingleton().GetActorVertexHasherTime1())
                        PerformanceLog(funcName, true, true, actorHash.size());
                    logger::trace("Check hashes done {}", actorHash.size());
                }
				isDetecting.store(false);
			});
		}
		else
		{
			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(std::string(__func__), false, true);

            std::lock_guard lg(actorHashLock);
            concurrency::concurrent_vector<RE::FormID> garbages;
            concurrency::parallel_for_each(actorHash.begin(), actorHash.end(), [&](auto& map) {
				if (!actorHashFunc(map.first, map.second))
                    garbages.push_back(map.first);
			});
            for (auto& id : garbages)
            {
                actorHash.erase(id);
            }
            logger::trace("Check hashes done {}", actorHash.size());

			if (Config::GetSingleton().GetActorVertexHasherTime1())
                PerformanceLog(std::string(__func__), true, true, actorHash.size());
		}
	}

	bool ActorVertexHasher::GetHash(RE::Actor* a_actor, GeometryHash& data)
	{
		if (IsInvalidActor(a_actor))
			return false;
		if (IsBlocked(a_actor->formID))
            return false;

		if (Config::GetSingleton().GetActorVertexHasherTime2())
			PerformanceLog(std::string(__func__), false, false);

		std::unordered_set<RE::BSGeometry*> existGeometries;
		auto root = a_actor->loadedData->data3D;
		RE::BSVisit::TraverseScenegraphGeometries(root.get(), [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (IsBlocked(a_actor->formID))
				return RE::BSVisit::BSVisitControl::kStop;
			if (!geo || geo->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
            auto found = data.find(geo);
			if (found == data.end())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (found->second->Update(geo))
				existGeometries.insert(geo);
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		for (auto it = data.begin(); it != data.end();)
		{
			if (existGeometries.find(it->first) == existGeometries.end())
				it = data.erase(it);
			else
				it++;
        }

		if (Config::GetSingleton().GetActorVertexHasherTime2())
			PerformanceLog(std::string(__func__), true, false);

		if (IsBlocked(a_actor->formID))
			return false;
		return true;
	}

	bool ActorVertexHasher::Hash::Update(RE::BSGeometry* a_geo)
	{
		if (!a_geo)
			return false;
		const RE::BSTriShape* triShape = a_geo->AsTriShape();
		if (!triShape)
			return false;
		std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
		const RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(a_geo);
		if (!skinPartition)
			return false;
		vertexCount = vertexCount > 0 ? vertexCount : skinPartition->vertexCount;

		Reset();
		if (const RE::BSDynamicTriShape* dynamicTriShape = a_geo->AsDynamicTriShape(); dynamicTriShape)
		{
			if (Config::GetSingleton().GetRealtimeDetectHead() == 1)
			{
				const auto morphData = GeometryData::GetMorphExtraData(a_geo);
				if (morphData)
				{
					Update(morphData->vertexData, sizeof(RE::NiPoint3) * vertexCount);
				}
			}
			else if (Config::GetSingleton().GetRealtimeDetectHead() == 2)
			{
				Update(dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData, sizeof(DirectX::XMVECTOR) * vertexCount);
			}
		}
		else
		{
			auto desc = a_geo->GetGeometryRuntimeData().vertexDesc;
			if (!desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) || !desc.HasFlag(RE::BSGraphics::Vertex::VF_UV))
				return false;
			Update(skinPartition->partitions[0].buffData->rawVertexData, sizeof(std::uint8_t) * vertexCount * desc.GetSize());
		}

		if (isCheckTexture)
        {
            using State = RE::BSGeometry::States;
            using Feature = RE::BSShaderMaterial::Feature;
            auto effect = a_geo->GetGeometryRuntimeData().properties[State::kEffect].get();
            auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(effect);
            RE::BSLightingShaderMaterialBase* material = skyrim_cast<RE::BSLightingShaderMaterialBase*>(lightingShader->material);
            if (material && material->normalTexture && !material->normalTexture->name.empty())
            {
                isMDNMTexture = IsCreatedByMDNM(material->normalTexture->name.c_str());
            }
        }
		GetNewHash();
		return true;
	}
}
