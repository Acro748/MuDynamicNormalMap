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
		if (currentTime - lastTickTime < Config::GetSingleton().GetDetectTickMS())
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

	bool ActorVertexHasher::Register(RE::Actor* a_actor, bSlot bipedSlot)
	{
		if (!Config::GetSingleton().GetRealtimeDetect())
			return true;
		if (bipedSlot == RE::BIPED_OBJECT::kHead && Config::GetSingleton().GetRealtimeDetectHead() == 0)
			return true;
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return false;
		ActorHashLock.lock_shared();
		if (ActorHash.find(a_actor->formID) == ActorHash.end())
		{
			auto condition = ConditionManager::GetSingleton().GetCondition(a_actor);
			ActorHash[a_actor->formID].IsDynamicTriShapeAsHead = condition.DynamicTriShapeAsHead;
		}
		auto found = ActorHash[a_actor->formID].hash.find(bipedSlot);
		if (found == ActorHash[a_actor->formID].hash.end())
		{
			ActorHash[a_actor->formID].hash.insert(std::make_pair(bipedSlot, std::make_shared<Hash>()));
			logger::debug("{:x} {} {}: registered for ActorVertexHasher", a_actor->formID, a_actor->GetName(), bipedSlot);
		}
		ActorHashLock.unlock_shared();
		return true;
	}
	bool ActorVertexHasher::InitialHash(RE::Actor* a_actor, bSlot bipedSlot)
	{
		if (!Config::GetSingleton().GetRealtimeDetect())
			return true;
		if (bipedSlot == RE::BIPED_OBJECT::kHead && Config::GetSingleton().GetRealtimeDetectHead() == 0)
			return true;
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return false;
		ActorHashLock.lock_shared();
		if (ActorHash.find(a_actor->formID) == ActorHash.end())
			return true;
		auto found = ActorHash[a_actor->formID].hash.find(bipedSlot);
		if (found != ActorHash[a_actor->formID].hash.end())
		{
			found->second->hashValue = 0;
			logger::debug("{:x} {} {}: initialized hash for ActorVertexHasher", a_actor->formID, a_actor->GetName(), bipedSlot);
		}
		ActorHashLock.unlock_shared();
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

				if (Config::GetSingleton().GetActorVertexHasherTime1())
					PerformanceLog(std::string(__func__), false, true);

				std::vector<std::future<void>> processes;
				auto ActorHash_ = ActorHash;
				for(auto& map : ActorHash_) {
					processes.push_back(processingThreads->submitAsync([&, map]() {
						RE::Actor* actor = GetFormByID<RE::Actor*>(map.first);
						if (!actor || !actor->loadedData || !actor->loadedData->data3D)
						{
							ActorHashLock.lock();
							ActorHash.unsafe_erase(map.first);
							ActorHashLock.unlock();
							return;
						}

						class ActorGuard {
							RE::Actor* actor = nullptr;
						public:
							ActorGuard() = delete;
							ActorGuard(RE::Actor* a_actor) : actor(a_actor) { actor->IncRefCount(); };
							~ActorGuard() { actor->DecRefCount(); };
						};
						ActorGuard ag(actor);

						if (BlockActors[actor->formID])
							return;
						if (!isPlayer(actor->formID) && (Config::GetSingleton().GetDetectDistance() > floatPrecision && playerPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) > Config::GetSingleton().GetDetectDistance()))
							return;
						if (!GetHash(actor, map.second))
							return;

						std::uint32_t bipedSlot = 0;
						for (auto& hashmap : map.second.hash)
						{
							std::size_t hash = hashmap.second->GetNewHash();
							hashmap.second->Reset();
							if (hashmap.second->hashValue != 0 && hash != 0)
							{
								if (hashmap.second->hashValue != hash)
								{
									bipedSlot |= 1 << hashmap.first;
									logger::trace("{:x} {} {} : found different hash {:x}", actor->formID, actor->GetName(), hashmap.first, hash);
								}
							}
							hashmap.second->hashValue = hash;
						}
						if (bipedSlot > 0)
						{
							logger::debug("{:x} {} : detected changed slot {:x}", actor->formID, actor->GetName(), bipedSlot);
							TaskManager::GetSingleton().QUpdateNormalMap(actor, bipedSlot);
						}
					}));
				}
				for (auto& process : processes)
				{
					process.get();
				}
				
				if (Config::GetSingleton().GetActorVertexHasherTime1())
					PerformanceLog(std::string(__func__), true, true, ActorHash.size());

				isDetecting.store(false);
				logger::trace("Check hashes done {}", ActorHash.size());
			});
		}
		else
		{
			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(std::string(__func__), false, true);

			auto ActorHash_ = ActorHash;
			concurrency::parallel_for_each(ActorHash_.begin(), ActorHash_.end(), [&](auto& map) {
				RE::Actor* actor = GetFormByID<RE::Actor*>(map.first);
				if (!actor || !actor->loadedData || !actor->loadedData->data3D)
				{
					ActorHashLock.lock();
					ActorHash.unsafe_erase(map.first);
					ActorHashLock.unlock();
					return;
				}
				if (!isPlayer(actor->formID) && (Config::GetSingleton().GetDetectDistance() > floatPrecision && playerPosition.GetSquaredDistance(actor->loadedData->data3D->world.translate) > Config::GetSingleton().GetDetectDistance()))
					return;
				if (!GetHash(actor, map.second))
					return;

				std::uint32_t bipedSlot = 0;
				for (auto& hashmap : map.second.hash)
				{
					std::size_t hash = hashmap.second->GetNewHash();
					hashmap.second->Reset();
					if (hashmap.second->hashValue != 0 && hash != 0)
					{
						if (hashmap.second->hashValue != hash)
						{
							bipedSlot |= 1 << hashmap.first;
							logger::trace("{:x} {} {} : found different hash {:x}", actor->formID, actor->GetName(), hashmap.first, hash);
						}
					}
					hashmap.second->hashValue = hash;
				}
				if (bipedSlot > 0)
				{
					logger::debug("{:x} {} : detected changed slot {:x}", actor->formID, actor->GetName(), bipedSlot);
					TaskManager::GetSingleton().QUpdateNormalMap(actor, bipedSlot);
				}
			});
			logger::trace("Check hashes done {}", ActorHash.size());

			if (Config::GetSingleton().GetActorVertexHasherTime1())
				PerformanceLog(std::string(__func__), true, true, ActorHash.size());
		}
	}

	bool ActorVertexHasher::GetHash(RE::Actor* a_actor, GeometryHash hashMap)
	{
		if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
			return false;
		if (BlockActors[a_actor->formID])
			return false;

		if (Config::GetSingleton().GetActorVertexHasherTime2())
			PerformanceLog(std::string(__func__), false, false);

		auto root = a_actor->loadedData->data3D.get();
		root->IncRefCount();
		RE::BSVisit::TraverseScenegraphGeometries(root, [&](RE::BSGeometry* geo) -> RE::BSVisit::BSVisitControl {
			using State = RE::BSGeometry::States;
			using Feature = RE::BSShaderMaterial::Feature;
			if (BlockActors[a_actor->formID])
				return RE::BSVisit::BSVisitControl::kStop;
			if (!geo || geo->name.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			const RE::BSTriShape* triShape = geo->AsTriShape();
			if (!triShape)
				return RE::BSVisit::BSVisitControl::kContinue;
			if (IsContainString(geo->name.c_str(), "[Ovl") || IsContainString(geo->name.c_str(), "[SOvl") || IsContainString(geo->name.c_str(), "overlay")) //without overlay
				return RE::BSVisit::BSVisitControl::kContinue;
			if (!geo->GetGeometryRuntimeData().properties[RE::BSGeometry::States::kEffect])
				return RE::BSVisit::BSVisitControl::kContinue;
			auto effect = geo->GetGeometryRuntimeData().properties[State::kEffect].get();
			if (!effect)
				return RE::BSVisit::BSVisitControl::kContinue;
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
			std::string texturePath = lowLetter(GetTexturePath(material->textureSet.get(), RE::BSTextureSet::Texture::kNormal));
			if (texturePath.empty())
				return RE::BSVisit::BSVisitControl::kContinue;
			if (!geo->GetGeometryRuntimeData().skinInstance)
				return RE::BSVisit::BSVisitControl::kContinue;

			bSlot slot = 0;
			bool isDynamicTriShape = geo->AsDynamicTriShape() ? true : false;
			if (hashMap.IsDynamicTriShapeAsHead && isDynamicTriShape)
			{
				slot = RE::BIPED_OBJECT::kHead;
			}
			else
			{
				auto skinInstance = geo->GetGeometryRuntimeData().skinInstance.get();
				auto dismember = netimmerse_cast<RE::BSDismemberSkinInstance*>(skinInstance);
				if (dismember)
				{
					std::int32_t pslot = -1;
					for (std::int32_t p = 0; p < dismember->GetRuntimeData().numPartitions; p++)
					{
						pslot = dismember->GetRuntimeData().partitions[p].slot;
						if (pslot < 30 || pslot >= RE::BIPED_OBJECT::kEditorTotal + 30)
						{
							if (isDynamicTriShape) { //maybe head
								slot = RE::BIPED_OBJECT::kHead;
							}
							else if (pslot == 0) //BP_TORSO
							{
								slot = RE::BIPED_OBJECT::kBody;
							}
							else //unknown slot
								continue;
						}
						else
							slot = pslot - 30;
						if (hashMap.hash.find(slot) != hashMap.hash.end())
							break;
					}
				}
				else //maybe it's just skinInstance in headpart or wrong mesh
				{
					slot = isDynamicTriShape ? RE::BIPED_OBJECT::kHead : RE::BIPED_OBJECT::kBody;
				}
			}

			auto found = hashMap.hash.find(slot);
			if (found == hashMap.hash.end())
				return RE::BSVisit::BSVisitControl::kContinue;

			std::uint32_t vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
			const RE::NiPointer<RE::NiSkinPartition> skinPartition = GetSkinPartition(geo);
			if (!skinPartition)
				return RE::BSVisit::BSVisitControl::kContinue;
			vertexCount = vertexCount > 0 ? vertexCount : skinPartition->vertexCount;

			if (const RE::BSDynamicTriShape* dynamicTriShape = geo->AsDynamicTriShape(); dynamicTriShape)
			{
				if (Config::GetSingleton().GetRealtimeDetectHead() == 1)
				{
					const auto morphData = GeometryData::GetMorphExtraData(geo);
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
				auto desc = geo->GetGeometryRuntimeData().vertexDesc;
				if (!desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) || !desc.HasFlag(RE::BSGraphics::Vertex::VF_UV))
					return RE::BSVisit::BSVisitControl::kContinue;
				found->second->Update(skinPartition->partitions[0].buffData->rawVertexData, sizeof(std::uint8_t) * vertexCount * desc.GetSize());
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});
		root->DecRefCount();

		if (Config::GetSingleton().GetActorVertexHasherTime2())
			PerformanceLog(std::string(__func__), true, false);

		if (BlockActors[a_actor->formID])
			return false;
		return true;
	}
}