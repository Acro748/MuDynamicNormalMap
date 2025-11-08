#include "ActorVertexHasher.h"

namespace Mus {
	void ActorVertexHasher::Init()
	{
		if (const auto NiNodeEvent = SKSE::GetNiNodeUpdateEventSource(); NiNodeEvent)
			NiNodeEvent->AddEventSink<SKSE::NiNodeUpdateEvent>(this);
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
		actorHashLock.lock_shared();
		if (actorHash.find(a_actor->formID) == actorHash.end())
		{
			auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
			actorHash[a_actor->formID].IsDynamicTriShapeAsHead = condition.DynamicTriShapeAsHead;
		}
		auto found = actorHash[a_actor->formID].hash.find(a_geo);
		if (found == actorHash[a_actor->formID].hash.end())
		{
			actorHash[a_actor->formID].hash.insert(std::make_pair(a_geo, std::make_shared<Hash>(a_geo)));
			logger::debug("{:x} {} {}: registered for ActorVertexHasher", a_actor->formID, a_actor->GetName(), a_geo->name.c_str());
		}
		actorHashLock.unlock_shared();
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

		auto actorHashFunc = [&, funcName, cameraPosition](std::pair<RE::FormID, GeometryHashData> map) {
			RE::NiPointer<RE::Actor> actor = RE::NiPointer(GetFormByID<RE::Actor*>(map.first));
			if (!actor || !actor->loadedData || !actor->loadedData->data3D)
			{
				actorHashLock.lock();
				actorHash.unsafe_erase(map.first);
				actorHashLock.unlock();
				return;
			}
			if (Config::GetSingleton().GetActorVertexHasherTime2())
				PerformanceLog(funcName + "::" + GetHexStr(map.first), false, true);

			if (IsBlocked(actor->formID))
				return;
			if (!IsPlayer(actor->formID) && (Config::GetSingleton().GetDetectDistance() > floatPrecision && cameraPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) > Config::GetSingleton().GetDetectDistance()))
				return;
			if (!GetHash(actor.get(), map.second.hash))
				return;

			bSlotbit bipedSlot = 0;
			for (auto& hash : map.second.hash)
			{
				if (hash.second->GetHash() != hash.second->GetOldHash())
				{
					bSlot slot = nif::GetBipedSlot(hash.first, map.second.IsDynamicTriShapeAsHead);
					bipedSlot |= 1 << slot;
					logger::trace("{:x} {} {} : found different hash {:x}", actor->formID, actor->GetName(), slot, hash.second->GetHash());
				}
			}
			if (bipedSlot > 0)
			{
				logger::debug("{:x} {} : detected changed slot {:x}", actor->formID, actor->GetName(), bipedSlot);
				TaskManager::GetSingleton().QUpdateNormalMap(actor.get(), bipedSlot);
			}

			if (Config::GetSingleton().GetActorVertexHasherTime2())
				PerformanceLog(funcName + "::" + GetHexStr(map.first), true, true);
		};

		if (Config::GetSingleton().GetRealtimeDetectOnBackGround())
		{
			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(funcName, false, true);

			blockActorsLock.lock();
			blockActors.clear();
			blockActorsLock.unlock();

			backGroundHasherThreads->submitAsync([&, actorHashFunc, funcName]() {
				isDetecting.store(true);

				actorHashLock.lock_shared();
				auto ActorHash_ = actorHash;
				actorHashLock.unlock_shared();

				for (auto& map : ActorHash_) {
					actorHashFunc(map);
				}
				
				if (Config::GetSingleton().GetActorVertexHasherTime1())
					PerformanceLog(funcName, true, true, ActorHash_.size());

				isDetecting.store(false);
				logger::trace("Check hashes done {}", ActorHash_.size());
			});
		}
		else
		{
			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(std::string(__func__), false, true);

			actorHashLock.lock_shared();
			auto ActorHash_ = actorHash;
			actorHashLock.unlock_shared();
			concurrency::parallel_for_each(ActorHash_.begin(), ActorHash_.end(), [&](auto& map) {
				actorHashFunc(map);
			});
			logger::trace("Check hashes done {}", ActorHash_.size());

			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(std::string(__func__), true, true, ActorHash_.size());
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
		auto data_ = data;
		auto root = a_actor->loadedData->data3D;
		RE::BSVisit::TraverseScenegraphGeometries(root.get(), [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (IsBlocked(a_actor->formID))
				return RE::BSVisit::BSVisitControl::kStop;
			if (!geo || geo->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			auto found = data_.find(geo);
			if (found == data.end())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (found->second->Update(geo))
				existGeometries.insert(geo);
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		for (auto& d : data_) {
			if (existGeometries.find(d.first) != existGeometries.end())
				continue;
			data.unsafe_erase(d.first);
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
		GetNewHash();
		return true;
	}
}