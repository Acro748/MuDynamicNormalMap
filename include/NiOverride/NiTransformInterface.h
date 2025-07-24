#pragma once

#include "IPluginInterface.h"
#include "OverrideVariant.h"
#include "OverrideInterface.h"
#include "SafeDataHolder.h"

#include <RE/N/NiTransform.h>

#include <unordered_map>

class NodeTransformKeys : public std::unordered_map<StringTableItem, OverrideRegistration<StringTableItem>>
{
public:
};

class NodeTransformRegistrationMapHolder : public SafeDataHolder<std::unordered_map<std::uint64_t, MultiRegistration<MultiRegistration<NodeTransformKeys, 2>, 2>>>
{
	friend class NiTransformInterface;
public:
	typedef std::unordered_map<std::uint64_t, MultiRegistration<MultiRegistration<NodeTransformKeys, 2>, 2>> RegMap;
};

class NodeTransformCache : public SafeDataHolder<std::unordered_map<SKEEFixedString, std::unordered_map<SKEEFixedString, RE::NiTransform>>>
{
	friend class NiTransformInterface;
public:
	typedef std::unordered_map<SKEEFixedString, RE::NiTransform> NodeMap;
	typedef std::unordered_map<SKEEFixedString, NodeMap> RegMap;
};

class NiTransformInterface : public IPluginInterface
{
public:
	virtual std::uint32_t GetVersion() = 0;

	virtual void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion) = 0;
	virtual bool Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap & stringTable) = 0;
	virtual void Revert() = 0;

	virtual bool AddNodeTransform(RE::TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, OverrideVariant & value) = 0;
	virtual bool RemoveNodeTransformComponent(RE::TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, std::uint16_t key, std::uint16_t index) = 0;
	virtual bool RemoveNodeTransform(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name) = 0;
	virtual void RemoveAllReferenceTransforms(RE::TESObjectREFR * refr) = 0;
	
	virtual OverrideVariant GetOverrideNodeValue(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, std::uint16_t key, std::int8_t index) = 0;
	virtual bool GetOverrideNodeTransform(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, std::uint16_t key, RE::NiTransform* result) = 0;

	virtual void GetOverrideTransform(OverrideSet * set, std::uint16_t key, RE::NiTransform * result) = 0;
	virtual SKEEFixedString GetRootModelPath(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale) = 0;

	virtual void UpdateNodeAllTransforms(RE::TESObjectREFR * ref) = 0;

	virtual void VisitNodes(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, std::function<bool(SKEEFixedString, OverrideRegistration<StringTableItem>*)> functor) = 0;
	virtual bool VisitNodeTransforms(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, RE::BSFixedString node, std::function<bool(OverrideRegistration<StringTableItem>*)> each_key, std::function<void(RE::NiNode*, RE::NiAVObject*, RE::NiTransform*)> finalize) = 0;
	virtual void UpdateNodeTransforms(RE::TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node) = 0;

	virtual void VisitStrings(std::function<void(SKEEFixedString)> functor) = 0;
	

	SKEEFixedString Impl_GetRootModelPath(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale)
	{
		RE::TESModel* model = NULL;
		RE::Character* character = skyrim_cast<RE::Character*>(refr);
		if (character) {
			RE::TESRace* race = character->GetRace();
			if (!race) {
				RE::TESNPC* actorBase = character->GetActorBase();
				if (actorBase)
					race = actorBase->GetRace();
			}

			if (race)
				model = &race->skeletonModels[isFemale ? 1 : 0];
		}
		else
			model = skyrim_cast<RE::TESModel*>(refr->data.objectReference);

		if (model)
			return SKEEFixedString(model->GetModel());

		return SKEEFixedString("");
	}

	bool InsertBaseTransform(SKEEFixedString rootModel, SKEEFixedString nodeName, RE::NiTransform transform = RE::NiTransform()) {
		SimpleLocker lock(&transformCache.m_lock);
		if (auto it = transformCache.m_data.find(rootModel); it != transformCache.m_data.end())
		{
			it->second[nodeName] = transform;
			return true;
		}
		return false;
	}
	bool HasBaseTransform(SKEEFixedString rootModel) {
		SimpleLocker lock(&transformCache.m_lock);
		auto it = transformCache.m_data.find(rootModel);
		return it != transformCache.m_data.end();
	}
	bool HasBaseTransform(SKEEFixedString rootModel, SKEEFixedString nodeName) {
		SimpleLocker lock(&transformCache.m_lock);
		if (auto it = transformCache.m_data.find(rootModel); it != transformCache.m_data.end())
		{
			return it->second.find(nodeName) != it->second.end();
		}
		return false;
	}
	RE::NiTransform* GetBaseTransform(SKEEFixedString rootModel, SKEEFixedString nodeName) {
		SimpleLocker lock(&transformCache.m_lock);
		if (auto it = transformCache.m_data.find(rootModel); it != transformCache.m_data.end())
		{
			if (auto found = it->second.find(nodeName); found != it->second.end())
			{
				RE::NiTransform result = found->second;
				return &result;
			}
		}
		return nullptr;
	}

	NodeTransformRegistrationMapHolder	transformData;
	NodeTransformCache					transformCache;
};