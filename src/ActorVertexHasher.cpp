#include "ActorVertexHasher.h"

namespace Mus {
//#define HASHER_TEST1
//#define HASHER_TEST2

	void ActorVertexHasher::Init()
	{
		if (const auto NiNodeEvent = SKSE::GetNiNodeUpdateEventSource(); NiNodeEvent)
			NiNodeEvent->AddEventSink<SKSE::NiNodeUpdateEvent>(this);
	}

	void ActorVertexHasher::onEvent(const FrameEvent& e)
	{
		if (IsSaveLoading.load())
			return;
		if (IsGamePaused.load() && !IsRaceSexMenu.load())
			return;
		if (isDetecting.load())
			return;
		auto currentTime = std::clock();
		if (Config::GetSingleton().GetDetectTickMS() <= currentTime - beforeDetectTickMS)
		{
			CheckingActorHash();
			beforeDetectTickMS = currentTime;
		}
	}

	void ActorVertexHasher::onEvent(const FacegenNiNodeEvent& e)
	{
		if (!e.facegenNiNode)
			return;
		RE::Actor* actor = skyrim_cast<RE::Actor*>(e.facegenNiNode->GetUserData());
		if (!actor)
			return;
		BlockActors[actor->formID] = true;
	}

	void ActorVertexHasher::onEvent(const ActorChangeHeadPartEvent& e)
	{
		if (!e.actor)
			return;
		BlockActors[e.actor->formID] = true;
	}

	void ActorVertexHasher::onEvent(const ArmorAttachEvent& e)
	{
		if (e.hasAttached)
			return;
		if (!e.actor)
			return;
		BlockActors[e.actor->formID] = true;
	}

	EventResult ActorVertexHasher::ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*)
	{
		if (!evn || !evn->reference)
			return EventResult::kContinue;
		BlockActors[evn->reference->formID] = true;
		return EventResult::kContinue;
	}

	bool ActorVertexHasher::Register(RE::Actor* a_actor, RE::BIPED_OBJECT bipedSlot)
	{
		if (!Config::GetSingleton().GetRealtimeDetect())
			return true;
		if (bipedSlot == RE::BIPED_OBJECT::kHead && Config::GetSingleton().GetRealtimeDetectHead() == 0)
			return true;
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return false;
		ActorHashLock.lock_shared();
		auto found = ActorHash[a_actor->formID].find(bipedSlot);
		if (found != ActorHash[a_actor->formID].end())
			found->second->hashValue = 0;
		else
		{
			ActorHash[a_actor->formID][bipedSlot] = std::make_shared<Hash>();
		}
		ActorHashLock.unlock_shared();
		logger::debug("{:x} {} {}: registered for ActorVertexHasher", a_actor->formID, a_actor->GetName(), std::to_underlying(bipedSlot));
		return true;
	}

	void ActorVertexHasher::CheckingActorHash()
	{
		if (ActorHash.size() == 0)
			return;
		auto p = RE::PlayerCharacter::GetSingleton();
		if (!p || !p->loadedData || !p->loadedData->data3D)
			return;
		auto playerPosition = p->loadedData->data3D->world.translate;

		if (Config::GetSingleton().GetRealtimeDetectOnBackGround())
		{
			if (!BackGroundHasher)
				BackGroundHasher = std::make_unique<ThreadPool_ParallelModule>(1);

			BlockActors.clear();

			BackGroundHasher->submitAsync([&]() {
				isDetecting.store(true);
#ifdef HASHER_TEST1
				PerformanceLog(std::string(__func__), false, true);
#endif // HASHER_TEST1
				concurrency::concurrent_vector<RE::FormID> garbages;
				std::vector<std::future<void>> processes;
				auto ActorHash_ = ActorHash;
				for(auto& map : ActorHash_) {
					processes.push_back(processingThreads->submitAsync([&, map]() {
						RE::TESForm* form = GetFormByID(map.first);
						if (!form || !form->Is(RE::FormType::ActorCharacter))
						{
							garbages.push_back(map.first);
							return;
						}
						RE::Actor* actor = skyrim_cast<RE::Actor*>(form);
						if (!actor || !actor->loadedData || !actor->loadedData->data3D)
						{
							garbages.push_back(map.first);
							return;
						}
						if (BlockActors[actor->formID])
							return;
						if (!isPlayer(actor->formID) && playerPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) > Config::GetSingleton().GetDetectDistance())
							return;
						if (!GetHash(actor, map.second))
							return;

						std::uint32_t bipedSlot = 0;
						for (auto& hashmap : map.second)
						{
							std::size_t hash = hashmap.second->GetNewHash();
							hashmap.second->Reset();
							if (hashmap.second->hashValue != 0 && hash != 0)
							{
								if (hashmap.second->hashValue != hash)
								{
									bipedSlot |= 1 << hashmap.first;
									logger::trace("{:x} {} {} : found different hash {:x}", actor->formID, actor->GetName(), std::to_underlying(hashmap.first), hash);
								}
							}
							hashmap.second->hashValue = hash;
						}
						ActorQueue[actor->formID] |= bipedSlot;
						if (ActorQueue[actor->formID] != 0)
						{
							if (TaskManager::GetSingleton().QUpdateNormalMap(actor, bipedSlot))
							{
								ActorQueue[actor->formID] = 0;
							}
						}
					}));
				}
				for (auto& process : processes)
				{
					process.get();
				}
				ActorHashLock.lock();
				for (auto& garbage : garbages)
				{
					ActorHash.unsafe_erase(garbage);
					ActorQueue.unsafe_erase(garbage);
				}
				ActorHashLock.unlock();
#ifdef HASHER_TEST1
				PerformanceLog(std::string(__func__), true, true, ActorHash.size());
#endif // HASHER_TEST1
				isDetecting.store(false);
			});
		}
		else
		{
#ifdef HASHER_TEST1
			PerformanceLog(std::string(__func__), false, true);
#endif // HASHER_TEST1
			concurrency::concurrent_vector<RE::FormID> garbages;
			auto ActorHash_ = ActorHash;
			concurrency::parallel_for_each(ActorHash_.begin(), ActorHash_.end(), [&](auto& map) {
				RE::TESForm* form = GetFormByID(map.first);
				if (!form || !form->Is(RE::FormType::ActorCharacter))
				{
					garbages.push_back(map.first);
					return;
				}
				RE::Actor* actor = skyrim_cast<RE::Actor*>(form);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					garbages.push_back(map.first);
					return;
				}
				if (BlockActors[actor->formID])
					return;
				if (!isPlayer(actor->formID) && playerPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) > Config::GetSingleton().GetDetectDistance())
					return;
				if (!GetHash(actor, map.second))
					return;

				std::uint32_t bipedSlot = 0;
				for (auto& hashmap : map.second)
				{
					std::size_t hash = hashmap.second->GetNewHash();
					hashmap.second->Reset();
					if (hashmap.second->hashValue != 0 && hash != 0)
					{
						if (hashmap.second->hashValue != hash)
						{
							bipedSlot |= 1 << hashmap.first;
							logger::trace("{:x} {} {} : found different hash {:x}", actor->formID, actor->GetName(), std::to_underlying(hashmap.first), hash);
						}
					}
					hashmap.second->hashValue = hash;
				}
				ActorQueue[actor->formID] |= bipedSlot;
				if (ActorQueue[actor->formID] != 0)
				{
					if (TaskManager::GetSingleton().QUpdateNormalMap(actor, bipedSlot))
					{
						ActorQueue[actor->formID] = 0;
					}
				}
			 });
			ActorHashLock.lock();
			for (auto& garbage : garbages)
			{
				ActorHash.unsafe_erase(garbage);
				ActorQueue.unsafe_erase(garbage);
			}
			ActorHashLock.unlock();
#ifdef HASHER_TEST1
			PerformanceLog(std::string(__func__), true, true, ActorHash.size());
#endif // HASHER_TEST1
		}
	}

	bool ActorVertexHasher::GetHash(RE::Actor* a_actor, GeometryHash hash)
	{
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return false;
		if (BlockActors[a_actor->formID])
			return false;
#ifdef HASHER_TEST2
		PerformanceLog(std::string(__func__), false, false);
#endif // HASHER_TEST2
		auto root = a_actor->loadedData->data3D.get();
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geometry) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (BlockActors[a_actor->formID])
				return RE::BSVisit::BSVisitControl::kStop;
			if (!geometry || geometry->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			const RE::BSTriShape* triShape = geometry->AsTriShape();
			if (!triShape)
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

			std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
			const RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(geometry);
			if (!skinPartition)
				return RE::BSVisit::BSVisitControl::kContinue;

			vertexCount = vertexCount > 0 ? vertexCount : skinPartition->vertexCount;
			if (const RE::BSDynamicTriShape* dynamicTriShape = geometry->AsDynamicTriShape(); dynamicTriShape)
			{
				auto found = hash.find(RE::BIPED_OBJECT::kHead);
				if (found == hash.end())
					return RE::BSVisit::BSVisitControl::kContinue;
				if (Config::GetSingleton().GetRealtimeDetectHead() == 1)
				{
					const auto morphData = GeometryData::GetMorphExtraData(geometry);
					if (morphData)
					{
						found->second->Update(morphData->vertexData, sizeof(RE::NiPoint3) * vertexCount);
					}
				}
				else if (Config::GetSingleton().GetRealtimeDetectHead() == 2)
				{
					found->second->Update(dynamicTriShape->GetDynamicTrishapeRuntimeData().dynamicData, sizeof(DirectX::XMFLOAT4) * vertexCount);
				}
			}
			else
			{
				auto desc = geometry->GetGeometryRuntimeData().vertexDesc;
				if (!desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) || !desc.HasFlag(RE::BSGraphics::Vertex::VF_UV))
					return RE::BSVisit::BSVisitControl::kContinue;

				RE::BIPED_OBJECT pslot = RE::BIPED_OBJECT::kNone;
				auto skinInstance = geometry->GetGeometryRuntimeData().skinInstance.get();
				auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance);
				if (!dismember)
					return RE::BSVisit::BSVisitControl::kContinue;
				bool found = false;
				for (std::uint32_t p = 0; p < dismember->GetRuntimeData().numPartitions; p++)
				{
					auto& partition = dismember->GetRuntimeData().partitions[p];
					if (partition.slot < 30 || partition.slot >= RE::BIPED_OBJECT::kEditorTotal + 30)
						return RE::BSVisit::BSVisitControl::kContinue; //unknown slot
					else
						pslot = RE::BIPED_OBJECT(partition.slot - 30);
					if (hash.find(pslot) != hash.end())
					{
						found = true;
						break;
					}
				}
				if (!found)
					return RE::BSVisit::BSVisitControl::kContinue;
				hash[pslot]->Update(skinPartition->partitions[0].buffData->rawVertexData, sizeof(std::uint8_t) * vertexCount * desc.GetSize());
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
#ifdef HASHER_TEST2
		PerformanceLog(std::string(__func__), true, false);
#endif // HASHER_TEST2
		if (BlockActors[a_actor->formID])
			return false;
		return true;
	}
}