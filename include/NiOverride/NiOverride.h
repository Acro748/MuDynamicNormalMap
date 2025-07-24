#pragma once

#include "SafeDataHolder.h"
#include "NiTransformInterface.h"
#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "TintMaskInterface.h"
#include "FaceMorphInterface.h"

class NiOverrideAPI {
public:
	NiOverrideAPI() {};
	~NiOverrideAPI() {};
	
	inline InterfaceExchangeMessage& GetExchangeMessage() { return exchangeMessage; }

	inline bool IsNiOverrideLoaded() {
		if (!NiOverride_Check) {
			if (!REL::Module::IsVR())
				NiOverride = GetModuleHandle(L"skee64.dll");
			else
				NiOverride = GetModuleHandle(L"skeevr.dll");
			NiOverride_Check = true;
		}
		return NiOverride;
	}

	inline bool NiOverrideload() {
		if (!NiOverride_load.exchange(true))
		{
			if (IsNiOverrideLoaded())
			{
				const auto messagingInterface = SKSE::GetMessagingInterface();
				if (messagingInterface)
					messagingInterface->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface, &GetExchangeMessage(), 0, "SKEE");
			}
		}
		return GetExchangeMessage().interfaceMap ? true : false;
	}

	template<typename T>
	T* GetInterfaceAPI() { return reinterpret_cast<T*>(this); }

protected:
	inline std::int8_t VersionCheck(std::uint16_t major, std::uint16_t minor, std::uint16_t patch) //-1 smaller, 0 equel, 1 bigger
	{
		auto version = REL::Module::get().version();
		if (major == version.major())
		{
			if (minor == version.minor())
			{
				if (patch == version.patch())
					return 0;
				else if (patch > version.patch())
					return -1;
				else
					return 1;
			}
			else if (minor > version.minor())
				return -1;
			else
				return 1;
		}
		else
		{
			if (major > version.major())
				return -1;
			else
				return 1;
		}
	}

private:
	bool NiOverride = false;
	bool NiOverride_Check = false;
	std::atomic<bool> NiOverride_load = false;

	InterfaceExchangeMessage exchangeMessage;
};



class BodyMorphInterfaceAPI :
	public NiOverrideAPI
{
	IPluginInterface* bodymorphInterface = nullptr;
	const std::string interfaceName_ = "BodyMorph";
	const std::string keyName_ = "";
public:
	BodyMorphInterfaceAPI(std::string keyName, std::string interfaceName = "BodyMorph") : keyName_(keyName), interfaceName_(interfaceName) {};
	~BodyMorphInterfaceAPI() {};

	inline const char* GetKeyName() { return keyName_.c_str(); }

	inline bool BodyMorphInterfaceload() {
		if (!bodymorphInterface && NiOverrideload())
		{
			bodymorphInterface = GetExchangeMessage().interfaceMap->QueryInterface(interfaceName_.c_str());
		}
		return bodymorphInterface ? true : false;
	}

	inline std::uint32_t GetVersion() {
		if (BodyMorphInterfaceload())
		{
			return bodymorphInterface->GetVersion();
		}
		return 0;
	}

	inline IBodyMorphInterface* GetIBodyMorphInterface() {
		if (BodyMorphInterfaceload())
		{
			if (bodymorphInterface->GetVersion() == 4)
				return static_cast<IBodyMorphInterface*>(bodymorphInterface);
		}
		return nullptr;
	}

	void SetMorph(RE::TESObjectREFR* actor, const char* morphName, float relative) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->SetMorph(actor, morphName, GetKeyName(), relative);
	}
	float GetMorph(RE::TESObjectREFR* actor, const char* morphName) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->GetMorph(actor, morphName, GetKeyName());
	}
	void ClearMorph(RE::TESObjectREFR* actor, const char* morphName) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ClearMorph(actor, morphName, GetKeyName());
	}

	float GetBodyMorphs(RE::TESObjectREFR* actor, const char* morphName) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->GetBodyMorphs(actor, morphName);
	}
	void ClearBodyMorphNames(RE::TESObjectREFR* actor, const char* morphName) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ClearBodyMorphNames(actor, morphName);
	}

	void ClearMorphs(RE::TESObjectREFR* actor) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ClearMorphs(actor);
	}

	void ApplyVertexDiff(RE::TESObjectREFR* refr, RE::NiAVObject* object, bool erase = false) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ApplyVertexDiff(refr, object, erase);
	}

	void ApplyBodyMorphs(RE::TESObjectREFR* refr, bool deferUpdate = true) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ApplyBodyMorphs(refr, deferUpdate);
	}
	void UpdateModelWeight(RE::TESObjectREFR* refr, bool immediate = false) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->UpdateModelWeight(refr, immediate);
	}

	bool HasMorphs(RE::TESObjectREFR* actor) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->HasMorphs(actor);
	}
	std::uint32_t BodyMorphsSize(RE::TESObjectREFR* actor) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->EvaluateBodyMorphs(actor);
	}

	bool HasBodyMorph(RE::TESObjectREFR* actor, const char* morphName) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->HasBodyMorph(actor, morphName, GetKeyName());
	}
	bool HasBodyMorphName(RE::TESObjectREFR* actor, const char* morphName) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->HasBodyMorphName(actor, morphName);
	}
	bool HasBodyMorphKey(RE::TESObjectREFR* actor) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->HasBodyMorphKey(actor, GetKeyName());
	}
	void ClearBodyMorphKeys(RE::TESObjectREFR* actor) {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ClearBodyMorphKeys(actor, GetKeyName());
	}

	size_t ClearMorphCache() {
		if (IBodyMorphInterface* bmi = GetIBodyMorphInterface(); bmi)
			bmi->ClearMorphCache();
	}
};

class NiTransformInterfaceAPI :
	public NiOverrideAPI
{
	IPluginInterface* niTransformInterface = nullptr;
	const std::string interfaceName_ = "NiTransform";
	const std::string keyName_ = "internal";
public:
	NiTransformInterfaceAPI(std::string keyName = "internal", std::string interfaceName = "NiTransform") : keyName_(keyName), interfaceName_(interfaceName) {};
	~NiTransformInterfaceAPI() {};
	
	inline const char* GetKeyName() { return keyName_.c_str(); }

	inline bool NiTransformInterfaceload() {
		if (!niTransformInterface && NiOverrideload())
		{
			niTransformInterface = GetExchangeMessage().interfaceMap->QueryInterface(interfaceName_.c_str());
		}
		return niTransformInterface ? true : false;
	}

	inline std::uint32_t GetVersion() {
		if (NiTransformInterfaceload())
		{
			return niTransformInterface->GetVersion();
		}
		return 0;
	}

	inline NiTransformInterface* GetNiTransformInterface() {
		if (NiTransformInterfaceload())
		{
			if (niTransformInterface->GetVersion() == 2)
				return static_cast<NiTransformInterface*>(niTransformInterface);
		}
		return nullptr;
	}
	inline INiTransformInterface* GetINiTransformInterface() {
		if (NiTransformInterfaceload())
		{
			if (niTransformInterface->GetVersion() == 3)
				return static_cast<INiTransformInterface*>(niTransformInterface);
		}
		return nullptr;
	}

	bool HasNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformPosition;
			RE::NiTransform result;
			return nti->GetOverrideNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), param, &result);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			return nti->HasNodeTransformPosition(ref, firstPerson, isFemale, node, GetKeyName());
		}
		return false;
	}
	bool HasNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformRotation;
			RE::NiTransform result;
			return nti->GetOverrideNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), param, &result);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			return nti->HasNodeTransformRotation(ref, firstPerson, isFemale, node, GetKeyName());
		}
		return false;
	}
	bool HasNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformScale;
			RE::NiTransform result;
			return nti->GetOverrideNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), param, &result);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			return nti->HasNodeTransformScale(ref, firstPerson, isFemale, node, GetKeyName());
		}
		return false;
	}
	bool HasNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformScaleMode;
			OverrideVariant overrideVariant = nti->GetOverrideNodeValue(ref, firstPerson, isFemale, node, name, param, 0);
			if (overrideVariant.type == OverrideVariant::kType_Int && overrideVariant.key == OverrideVariant::kParam_NodeTransformScale)
			{
				return true;
			}
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			return nti->HasNodeTransformScaleMode(ref, firstPerson, isFemale, node, GetKeyName());
		}
		return false;
	}

	void SetNodeTransform(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, RE::NiPoint3 value = RE::NiPoint3()) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformPosition;
			if (value != RE::NiPoint3())
			{
				float pos[3] = { value.x, value.y, value.z };
				OverrideVariant posV[3];
				for (std::uint32_t i = 0; i < 3; i++) {
					posV[i].SetFloat(param, i, pos[i]);
					nti->AddNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), posV[i]);
				}
			}
			else
			{
				for (std::uint32_t i = 0; i < 3; i++)
					nti->RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, GetKeyName(), param, i);
			}
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (value != RE::NiPoint3())
			{
				INiTransformInterface::Position pos = { value.x, value.y, value.z };
				nti->AddNodeTransformPosition(ref, firstPerson, isFemale, node, GetKeyName(), pos);
			}
			else
			{
				nti->RemoveNodeTransformPosition(ref, firstPerson, isFemale, node, GetKeyName());
			}
		}
	}
	void SetNodeTransform(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, RE::NiMatrix3 value = RE::NiMatrix3()) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformRotation;
			if (value != RE::NiMatrix3())
			{
				OverrideVariant rotV[9];
				for (std::uint32_t i = 0; i < 3; i++)
				{
					for (std::uint32_t j = 0; j < 3; j++)
					{
						rotV[(i * 3) + j].SetFloat(param, (i * 3) + j, value.entry[i][j]);
						nti->AddNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), rotV[(i * 3) + j]);
					}
				}
			}
			else
			{
				for (std::uint32_t i = 0; i < 9; i++)
					nti->RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, GetKeyName(), param, i);
			}
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (value != RE::NiMatrix3())
			{
				RE::NiPoint3 rot = util.GetEulerAngles(value);
				INiTransformInterface::Rotation rotV = { rot.y * util.toDegree, rot.z * util.toDegree, rot.x * util.toDegree };
				nti->AddNodeTransformRotation(ref, firstPerson, isFemale, node, GetKeyName(), rotV);
			}
			else
			{
				nti->RemoveNodeTransformRotation(ref, firstPerson, isFemale, node, GetKeyName());
			}
		}
	}
	void SetNodeTransform(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, float value = 1.0f) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformScale;
			if (value != 1.0f)
			{
				OverrideVariant scale;
				scale.SetFloat(param, 0, value);
				nti->AddNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), scale);
			}
			else
			{
				nti->RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, GetKeyName(), param, 0);
			}
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (value != 1.0f)
			{
				nti->AddNodeTransformScale(ref, firstPerson, isFemale, node, GetKeyName(), value);
			}
			else
			{
				nti->RemoveNodeTransformScale(ref, firstPerson, isFemale, node, GetKeyName());
			}
		}
	}
	void SetNodeTransform(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, std::uint32_t ScaleMode) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformScaleMode;
			OverrideVariant scaleModeVar;
			scaleModeVar.SetInt(param, 0, ScaleMode);
			nti->AddNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), scaleModeVar);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			nti->AddNodeTransformScaleMode(ref, firstPerson, isFemale, node, GetKeyName(), ScaleMode);
		}
	}
	void RemoveNodeTransform(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			nti->RemoveNodeTransform(ref, firstPerson, isFemale, node, GetKeyName());
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			nti->RemoveNodeTransform(ref, firstPerson, isFemale, node, GetKeyName());
		}
	}
	void RemoveAllReferenceTransforms(RE::TESObjectREFR* ref) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			nti->RemoveAllReferenceTransforms(ref);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			nti->RemoveAllReferenceTransforms(ref);
		}
	}

	RE::NiPoint3 GetNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformPosition;
			RE::NiTransform result;
			if (nti->GetOverrideNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), param, &result))
				return result.translate;
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (nti->HasNodeTransformPosition(ref, firstPerson, isFemale, node, GetKeyName()))
			{
				INiTransformInterface::Position pos = nti->GetNodeTransformPosition(ref, firstPerson, isFemale, node, GetKeyName());
				return RE::NiPoint3(pos.x, pos.y, pos.z);
			}
		}
		return RE::NiPoint3();
	}
	RE::NiMatrix3 GetNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformRotation;
			RE::NiTransform result;
			if (nti->GetOverrideNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), param, &result))
				return result.rotate;
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (nti->HasNodeTransformRotation(ref, firstPerson, isFemale, node, GetKeyName()))
			{
				INiTransformInterface::Rotation rot = nti->GetNodeTransformRotation(ref, firstPerson, isFemale, node, GetKeyName());
				return util.SetEulerAngles(rot.bank * util.toRadian, rot.heading * util.toRadian, rot.attitude * util.toRadian);
			}
		}
		return RE::NiMatrix3();
	}
	float GetNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformScale;
			RE::NiTransform result;
			if (nti->GetOverrideNodeTransform(ref, firstPerson, isFemale, node, GetKeyName(), param, &result))
				return result.scale;
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (nti->HasNodeTransformScale(ref, firstPerson, isFemale, node, GetKeyName()))
			{
				return nti->GetNodeTransformScale(ref, firstPerson, isFemale, node, GetKeyName());
			}
		}
		return 1.0f;
	}
	std::uint32_t GetNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			std::uint16_t param = OverrideVariant::kParam_NodeTransformScaleMode;
			OverrideVariant overrideVariant = nti->GetOverrideNodeValue(ref, firstPerson, isFemale, node, GetKeyName(), param, 0);
			if (overrideVariant.type == OverrideVariant::kType_Int && overrideVariant.key == OverrideVariant::kParam_NodeTransformScale)
			{
				return overrideVariant.data.u;
			}
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			if (nti->HasNodeTransformScaleMode(ref, firstPerson, isFemale, node, GetKeyName()))
			{
				return nti->GetNodeTransformScaleMode(ref, firstPerson, isFemale, node, GetKeyName());
			}
		}
		return 0;
	}

	void UpdateNodeTransforms(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			nti->UpdateNodeTransforms(ref, firstPerson, isFemale, node);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			nti->UpdateNodeTransforms(ref, firstPerson, isFemale, node);
		}
	}
	void UpdateNodeAllTransforms(RE::TESObjectREFR* ref) {
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			nti->UpdateNodeAllTransforms(ref);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			nti->UpdateNodeAllTransforms(ref);
		}
	}

	bool InsertBaseTransform(RE::TESObjectREFR* ref, const char* nodeName, RE::NiTransform transform = RE::NiTransform()) {
		RE::Actor* actor = skyrim_cast<RE::Actor*>(ref);
		RE::TESNPC* npc = actor ? actor->GetActorBase() : nullptr;
		if (!npc)
			return false;
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			return nti->InsertBaseTransform(nti->Impl_GetRootModelPath(ref, false, npc->GetSex()), nodeName, transform);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			IPluginInterface* pif = static_cast<IPluginInterface*>(nti);
			NiTransformInterface* base_nti = static_cast<NiTransformInterface*>(pif);
			if (base_nti)
			{
				return base_nti->InsertBaseTransform(base_nti->Impl_GetRootModelPath(ref, false, npc->GetSex()), nodeName, transform);
			}
		}
		return false;
	}
	bool HasBaseTransform(RE::TESObjectREFR* ref) {
		RE::Actor* actor = skyrim_cast<RE::Actor*>(ref);
		RE::TESNPC* npc = actor ? actor->GetActorBase() : nullptr;
		if (!npc)
			return false;
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			return nti->HasBaseTransform(nti->Impl_GetRootModelPath(ref, false, npc->GetSex()));
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			IPluginInterface* pif = static_cast<IPluginInterface*>(nti);
			NiTransformInterface* base_nti = static_cast<NiTransformInterface*>(pif);
			if (base_nti)
			{
				return base_nti->HasBaseTransform(base_nti->Impl_GetRootModelPath(ref, false, npc->GetSex()));
			}
		}
		return false;
	}
	bool HasBaseTransform(RE::TESObjectREFR* ref, const char* nodeName) {
		RE::Actor* actor = skyrim_cast<RE::Actor*>(ref);
		RE::TESNPC* npc = actor ? actor->GetActorBase() : nullptr;
		if (!npc)
			return false;
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			return nti->HasBaseTransform(nti->Impl_GetRootModelPath(ref, false, npc->GetSex()), nodeName);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			IPluginInterface* pif = static_cast<IPluginInterface*>(nti);
			NiTransformInterface* base_nti = static_cast<NiTransformInterface*>(pif);
			if (base_nti)
			{
				return base_nti->HasBaseTransform(base_nti->Impl_GetRootModelPath(ref, false, npc->GetSex()), nodeName);
			}
		}
		return false;
	}
	RE::NiTransform* GetBaseTransform(RE::TESObjectREFR* ref, const char* nodeName) {
		RE::Actor* actor = skyrim_cast<RE::Actor*>(ref);
		RE::TESNPC* npc = actor ? actor->GetActorBase() : nullptr;
		if (!npc)
			return nullptr;
		if (NiTransformInterface* nti = GetNiTransformInterface(); nti)
		{
			return nti->GetBaseTransform(nti->Impl_GetRootModelPath(ref, false, npc->GetSex()), nodeName);
		}
		else if (INiTransformInterface* nti = GetINiTransformInterface(); nti)
		{
			IPluginInterface* pif = static_cast<IPluginInterface*>(nti);
			NiTransformInterface* base_nti = static_cast<NiTransformInterface*>(pif);
			if (base_nti)
			{
				return base_nti->GetBaseTransform(base_nti->Impl_GetRootModelPath(ref, false, npc->GetSex()), nodeName);
			}
		}
		return nullptr;
	}

private:
	struct util {
		const double pi = 3.14159265358979323846;
		const float toDegree = 180 / pi;
		const float toRadian = pi / 180;
		RE::NiPoint3 GetEulerAngles(RE::NiMatrix3 a_angles)
		{
			RE::NiPoint3 a_point;
			if (a_angles.entry[1][0] > 0.998) { // singularity at north pole
				a_point.x = 0;
				a_point.y = atan2(a_angles.entry[0][2], a_angles.entry[2][2]);
				a_point.z = pi / 2;
			}
			else if (a_angles.entry[1][0] < -0.998) { // singularity at south pole
				a_point.x = 0;
				a_point.y = atan2(a_angles.entry[0][2], a_angles.entry[2][2]);
				a_point.z = -pi / 2;
			}
			else {
				a_point.x = atan2(-a_angles.entry[1][2], a_angles.entry[1][1]);
				a_point.y = atan2(-a_angles.entry[2][0], a_angles.entry[0][0]);
				a_point.z = asin(a_angles.entry[1][0]);
			}
			return a_point;
		}
		RE::NiMatrix3 SetEulerAngles(float a_point_x, float a_point_y, float a_point_z)
		{
			RE::NiMatrix3 a_angles;
			double cb = cos(a_point_x);
			double sb = sin(a_point_x);
			double ch = cos(a_point_y);
			double sh = sin(a_point_y);
			double ca = cos(a_point_z);
			double sa = sin(a_point_z);

			a_angles.entry[0][0] = ch * ca;
			a_angles.entry[0][1] = sh * sb - ch * sa * cb;
			a_angles.entry[0][2] = ch * sa * sb + sh * cb;
			a_angles.entry[1][0] = sa;
			a_angles.entry[1][1] = ca * cb;
			a_angles.entry[1][2] = -ca * sb;
			a_angles.entry[2][0] = -sh * ca;
			a_angles.entry[2][1] = sh * sa * cb + ch * sb;
			a_angles.entry[2][2] = -sh * sa * sb + ch * cb;
			return a_angles;
		}
	} util;
};

class OverlayInterfaceAPI :
	public NiOverrideAPI
{
	IPluginInterface* overlayInterface = nullptr;
	std::string interfaceName_ = "Overlay";
public:
	OverlayInterfaceAPI(std::string interfaceName = "Overlay") : interfaceName_(interfaceName) {};
	~OverlayInterfaceAPI() {};

	inline bool OverlayInterfaceload() {
		if (!overlayInterface && NiOverrideload())
		{
			overlayInterface = GetExchangeMessage().interfaceMap->QueryInterface(interfaceName_.c_str());
		}
		return overlayInterface ? true : false;
	}

	inline std::uint32_t GetVersion() {
		if (OverlayInterfaceload())
		{
			return overlayInterface->GetVersion();
		}
		return 0;
	}

	inline OverlayInterface* GetOverlayInterface() {
		if (OverlayInterfaceload())
		{
			if (overlayInterface->GetVersion() == 1)
				return static_cast<OverlayInterface*>(overlayInterface);
		}
		return nullptr;
	}

	bool HasOverlays(RE::TESObjectREFR* reference) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			return oi->HasOverlays(reference);
		}
		return false;
	}
	void AddOverlays(RE::TESObjectREFR* reference) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->AddOverlays(reference);
		}
	}
	void RemoveOverlays(RE::TESObjectREFR* reference) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RemoveOverlays(reference);
		}
	}
	void RevertOverlays(RE::TESObjectREFR* reference, bool resetDiffuse) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RevertOverlays(reference, resetDiffuse);
		}
	}
	void RevertOverlay(RE::TESObjectREFR* reference, RE::BSFixedString nodeName, std::uint32_t armorMask, std::uint32_t addonMask, bool resetDiffuse) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RevertOverlay(reference, nodeName, armorMask, addonMask, resetDiffuse);
		}
	}

	void EraseOverlays(RE::TESObjectREFR* reference) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->EraseOverlays(reference);
		}
	}

	void RevertHeadOverlays(RE::TESObjectREFR* reference, bool resetDiffuse) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RevertHeadOverlays(reference, resetDiffuse);
		}
	}
	void RevertHeadOverlay(RE::TESObjectREFR* reference, RE::BSFixedString nodeName, std::uint32_t partType, std::uint32_t shaderType, bool resetDiffuse) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RevertHeadOverlay(reference, nodeName, partType, shaderType, resetDiffuse);
		}
	}

	void SetupOverlay(std::uint32_t primaryCount, const char* primaryPath, const char* primaryNode, std::uint32_t secondaryCount, const char* secondaryPath, const char* secondaryNode, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->SetupOverlay(primaryCount, primaryPath, primaryNode, secondaryCount, secondaryPath, secondaryNode, refr, boneTree, resultNode);
		}
	}

	void UninstallOverlay(const char* nodeName, RE::TESObjectREFR* refr, RE::NiNode* parent) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->UninstallOverlay(nodeName, refr, parent);
		}
	}
	void InstallOverlay(const char* nodeName, const char* path, RE::TESObjectREFR* refr, RE::BSGeometry* source, RE::NiNode* destination, RE::BGSTextureSet* textureSet = NULL) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->InstallOverlay(nodeName, path, refr, source, destination, textureSet);
		}
	}
	void ResetOverlay(const char* nodeName, RE::TESObjectREFR* refr, RE::BSGeometry* source, RE::NiNode* destination, RE::BGSTextureSet* textureSet = NULL, bool resetDiffuse = false) { // Re-applies the skin's textures
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->ResetOverlay(nodeName, refr, source, destination, textureSet, resetDiffuse);
		}
	}
	std::string& GetDefaultTexture() {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			return oi->GetDefaultTexture();
		}
		std::string result = "";
		return result;
	}
	void SetDefaultTexture(const std::string& newTexture) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->SetDefaultTexture(newTexture);
		}
	}

	// Relinks an overlay node by name to the new source
	void RelinkOverlay(const char* nodeName, RE::TESObjectREFR* refr, RE::BSGeometry* source, RE::NiNode* skeleton) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RelinkOverlay(nodeName, refr, source, skeleton);
		}
	}

	// Relinks structured name of overlays to new source
	void RelinkOverlays(std::uint32_t primaryCount, const char* primaryNode, std::uint32_t secondaryCount, const char* secondaryNode, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RelinkOverlays(primaryCount, primaryNode, secondaryCount, secondaryNode, refr, boneTree, resultNode);
		}
	}

	// Builds default overlays
	void BuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->BuildOverlays(armorMask, addonMask, refr, boneTree, resultNode);
		}
	}

	// Relinks default overlays
	void RebuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) {
		if (OverlayInterface* oi = GetOverlayInterface(); oi)
		{
			oi->RebuildOverlays(armorMask, addonMask, refr, boneTree, resultNode);
		}
	}
};

class OverrideInterfaceAPI :
	public NiOverrideAPI
{
	IPluginInterface* overrideInterface = nullptr;
	const std::string interfaceName_ = "Override";
public:
	OverrideInterfaceAPI(std::string interfaceName = "Override") : interfaceName_(interfaceName) {};
	~OverrideInterfaceAPI() {};

	inline bool OverrideInterfaceload() {
		if (!overrideInterface && NiOverrideload())
		{
			overrideInterface = GetExchangeMessage().interfaceMap->QueryInterface(interfaceName_.c_str());
		}
		return overrideInterface ? true : false;
	}

	inline std::uint32_t GetVersion() {
		if (OverrideInterfaceload())
		{
			return overrideInterface->GetVersion();
		}
		return 0;
	}

	inline OverrideInterface* GetOverrideInterface() {
		if (OverrideInterfaceload())
		{
			if (overrideInterface->GetVersion() == 1)
				return static_cast<OverrideInterface*>(overrideInterface);
		}
		return nullptr;
	}
	inline IOverrideInterface* GetIOverrideInterface() {
		if (OverrideInterfaceload())
		{
			if (overrideInterface->GetVersion() == 2)
				return static_cast<IOverrideInterface*>(overrideInterface);
		}
		return nullptr;
	}

	enum Key {
		ShaderEmissiveColor, //int
		ShaderEmissiveMultiple, //float
		ShaderGlossiness, //float
		ShaderSpecularStrength, //float
		ShaderLightingEffect1, //float
		ShaderLightingEffect2, //float
		ShaderTextureSet, //TextureSet
		ShaderTintColor, //int
		ShaderAlpha, //float
		ShaderTexture, //string (index 0-8)
		ControllerStartStop, //float (-1.0 for stop, anything else indicates start time)
		ControllerStartTime, //float
		ControllerStopTime, //float
		ControllerFrequency, //float
		ControllerPhase, //float

		Total
	};

	class GetOverrideValue : public IOverrideInterface::GetVariant {

	};
	class SetOverrideValue : public IOverrideInterface::SetVariant {
		Type type;
		skee_i32 value1;
		float value2;
		std::string value3;
		bool value4;
		RE::BGSTextureSet* value5;
	public:
		SetOverrideValue(skee_i32 value) : type(Type::Int), value1(value) {};
		SetOverrideValue(float value) : type(Type::Float), value2(value) {};
		SetOverrideValue(const char* value) : type(Type::String), value3(value) {};
		SetOverrideValue(bool value) : type(Type::Bool), value4(value) {};
		SetOverrideValue(RE::BGSTextureSet* value) : type(Type::TextureSet), value5(value) {};

		virtual Type GetType() override { return type; }
		virtual skee_i32 Int() override { return value1; }
		virtual float Float() override { return value2; }
		virtual const char* String() override { return value3.c_str(); }
		virtual bool Bool() override { return value4; }
		virtual RE::BGSTextureSet* TextureSet() override { return value5; }
	};

	template <typename T>
	void AddRawOverride(RE::FormID formId, bool isFemale, RE::RefHandle armorHandle, RE::RefHandle addonHandle, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddRawOverride(formId, isFemale, armorHandle, addonHandle, nodeName, ovalue);
		}
	}
	template <typename T>
	void AddOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddOverride(refr, isFemale, armor, addon, nodeName, ovalue);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			auto ovalue = SetOverrideValue(value);
			oi->AddArmorOverride(refr, isFemale, armor, addon, nodeName.c_str(), key, index, ovalue);
		}
	}

	template <typename T>
	void AddRawNodeOverride(RE::RefHandle handle, bool isFemale, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddRawNodeOverride(handle, isFemale, nodeName, ovalue);
		}
	}
	template <typename T>
	void AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddNodeOverride(refr, isFemale, nodeName, ovalue);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			auto ovalue = SetOverrideValue(value);
			oi->AddNodeOverride(refr, isFemale, nodeName.c_str(), key, index, ovalue);
		}
	}

	template <typename T>
	void SetArmorAddonProperty(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->SetArmorAddonProperty(refr, isFemale, armor, addon, nodeName, ovalue, immediate);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			auto ovalue = SetOverrideValue(value);
			oi->SetArmorProperty(refr, isFemale, armor, addon, nodeName.c_str(), key, index, ovalue, immediate);
		}
	}
	template <typename T>
	void GetArmorAddonProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->GetArmorAddonProperty(refr, firstPerson, armor, addon, nodeName, ovalue);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			GetOverrideValue ovalue;
			oi->GetArmorProperty(refr, firstPerson, armor, addon, nodeName.c_str(), key, index, ovalue);
		}
	}

	template <typename T>
	void SetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value, bool immediate = false) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->SetNodeProperty(refr, nodeName, &ovalue, immediate);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			auto ovalue = SetOverrideValue(value);
			oi->SetNodeProperty(refr, firstPerson, nodeName.c_str(), key, index, ovalue, immediate);
		}
	}
	template <typename T>
	void GetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T& value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->GetNodeProperty(refr, firstPerson, nodeName, &ovalue);
		}
	}

	bool HasArmorAddonNode(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName, bool debug = false) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			return oi->HasArmorAddonNode(refr, firstPerson, armor, addon, nodeName, debug);
		}
	}

	void ApplyNodeOverrides(RE::TESObjectREFR* refr, RE::NiAVObject* object, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->ApplyNodeOverrides(refr, object, immediate);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			oi->ApplyNodeOverrides(refr, object, immediate);
		}
	}

	void ApplyOverrides(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->ApplyOverrides(refr, armor, addon, object, immediate);
		}
	}

	void RemoveAllOverrides() {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllOverrides();
		}
	}
	void RemoveAllReferenceOverrides(RE::TESObjectREFR* reference) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllReferenceOverrides(reference);
		}
	}

	void RemoveAllArmorOverrides(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllArmorOverrides(refr, isFemale, armor);
		}
	}
	void RemoveAllArmorAddonOverrides(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllArmorAddonOverrides(refr, isFemale, armor, addon);
		}
	}
	void RemoveAllArmorAddonNodeOverrides(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllArmorAddonNodeOverrides(refr, isFemale, armor, addon, nodeName);
		}
	}
	void RemoveArmorAddonOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveArmorAddonOverride(refr, isFemale, armor, addon, nodeName, key, index);
		}
	}

	void RemoveAllNodeOverrides() {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllNodeOverrides();
		}
	}
	void RemoveAllReferenceNodeOverrides(RE::TESObjectREFR* reference) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllReferenceNodeOverrides(reference);
		}
	}

	void RemoveAllNodeNameOverrides(RE::TESObjectREFR* refr, bool isFemale, RE::BSFixedString nodeName) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllNodeNameOverrides(refr, isFemale, nodeName);
		}
	}
	void RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveNodeOverride(refr, isFemale, nodeName, key, index);
		}
	}

	template <typename T>
	T GetOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant* value = oi->GetOverride(refr, isFemale, armor, addon, nodeName, key, index);
			T result;
			UnPackValue(&result, value);
			return result;
		}
	}
	template <typename T>
	T GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant* value = oi->GetNodeOverride(refr, isFemale, nodeName, key, index);
			T result;
			UnPackValue(&result, value);
			return result;
		}
	}

	template <typename T>
	void AddRawWeaponOverride(RE::RefHandle handle, bool isFemale, bool firstPerson, RE::RefHandle weaponHandle, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddRawWeaponOverride(handle, isFemale, firstPerson, weaponHandle, nodeName, ovalue);
		}
	}
	template <typename T>
	void AddWeaponOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, ovalue);
		}
	}
	template <typename T>
	T GetWeaponOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant* value = oi->GetWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index);
			T result;
			UnpackValue(result, value);
			return result;
		}
	}
	void ApplyWeaponOverrides(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectWEAP* weapon, RE::NiAVObject* object, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->ApplyWeaponOverrides(refr, firstPerson, weapon, object, immediate);
		}
	}

	void RemoveAllWeaponBasedOverrides() {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllWeaponBasedOverrides();
		}
	}
	void RemoveAllReferenceWeaponOverrides(RE::TESObjectREFR* reference) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllReferenceWeaponOverrides(reference);
		}
	}

	void RemoveAllWeaponOverrides(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP* weapon) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllWeaponOverrides(refr, isFemale, firstPerson, weapon);
		}
	}
	void RemoveAllWeaponNodeOverrides(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllWeaponNodeOverrides(refr, isFemale, firstPerson, weapon, nodeName);
		}
	}
	void RemoveWeaponOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveWeaponOverride(refr, isFemale, firstPerson, weapon, nodeName, key, index);
		}
	}

	bool HasWeaponNode(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName, bool debug = false) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			return oi->HasWeaponNode(refr, firstPerson, weapon, nodeName, debug);
		}
	}
	void SetWeaponProperties(RE::RefHandle handle, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->SetWeaponProperties(handle, immediate);
		}
	}

	template <typename T>
	void SetWeaponProperty(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->SetWeaponProperty(refr, isFemale, firstPerson, weapon, nodeName, ovalue, immediate);
		}
	}
	template <typename T>
	void GetWeaponProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectWEAP* weapon, RE::BSFixedString nodeName, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->GetWeaponProperty(refr, firstPerson, weapon, nodeName, ovalue);
		}
	}

	template <typename T>
	void AddRawSkinOverride(RE::RefHandle handle, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddRawSkinOverride(handle, isFemale, firstPerson, slotMask, ovalue);
		}
	}
	template <typename T>
	void AddSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->AddSkinOverride(refr, isFemale, firstPerson, slotMask, ovalue);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			auto ovalue = SetOverrideValue(value);
			oi->AddSkinOverride(refr, isFemale, firstPerson, slotMask, key, index, ovalue);
		}
	}
	template <typename T>
	T GetSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant* value = oi->GetSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
			T result;
			UnPackValue(&result, value);
			return result;
		}
	}
	void ApplySkinOverrides(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, std::uint32_t slotMask, RE::NiAVObject* object, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->ApplySkinOverrides(refr, firstPerson, armor, addon, slotMask, object, immediate);
		}
	}
	void RemoveAllSkinOverrides(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllSkinOverrides(refr, isFemale, firstPerson, slotMask);
		}
	}
	void RemoveSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveSkinOverride(refr, isFemale, firstPerson, slotMask, key, index);
		}
	}
	void SetSkinProperties(RE::TESObjectREFR* refr, bool immediate) {
		if (!refr)
			return;
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->SetSkinProperties(refr->formID, immediate);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			oi->SetSkinProperties(refr, immediate);
		}
	}
	void RemoveAllSkinBasedOverrides() {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllSkinBasedOverrides();
		}
	}
	void RemoveAllReferenceSkinOverrides(RE::TESObjectREFR* reference) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			oi->RemoveAllReferenceSkinOverrides(reference);
		}
	}
	template <typename T>
	void SetSkinProperty(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, T value, bool immediate) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->SetSkinProperty(refr, isFemale, firstPerson, slotMask, &ovalue, immediate);
		}
		else if (IOverrideInterface* oi = GetIOverrideInterface(); oi)
		{
			auto ovalue = SetOverrideValue(value);
			oi->SetSkinProperty(refr, firstPerson, slotMask, key, index, ovalue, immediate);
		}
	}
	template <typename T>
	void GetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, std::uint32_t slotMask, std::uint16_t key, std::uint8_t index, T value) {
		if (OverrideInterface* oi = GetOverrideInterface(); oi)
		{
			OverrideVariant ovalue;
			PackValue(&ovalue, key, index, &value);
			oi->GetSkinProperty(refr, firstPerson, slotMask, ovalue);
		}
	}
};

class TintMaskInterfaceAPI :
	public NiOverrideAPI
{
	IPluginInterface* tintMaskInterface = nullptr;
	const std::string interfaceName_ = "TintMask";
public:
	TintMaskInterfaceAPI(std::string interfaceName = "TintMask") : interfaceName_(interfaceName) {};
	~TintMaskInterfaceAPI() {};

	inline bool TintMaskInterfaceload() {
		if (!tintMaskInterface && NiOverrideload())
		{
			tintMaskInterface = GetExchangeMessage().interfaceMap->QueryInterface(interfaceName_.c_str());
		}
		return tintMaskInterface ? true : false;
	}

	inline std::uint32_t GetVersion() {
		if (TintMaskInterfaceload())
		{
			return tintMaskInterface->GetVersion();
		}
		return 0;
	}

	inline TintMaskInterface* GetTintMaskInterface() {
		if (TintMaskInterfaceload())
		{
			if (tintMaskInterface->GetVersion() == 0)
				return static_cast<TintMaskInterface*>(tintMaskInterface);
		}
		return nullptr;
	}

	void ApplyMasks(RE::TESObjectREFR* refr, bool isFirstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, std::uint32_t flags, ItemAttributeDataPtr overrides, LayerFunctor layer = LayerFunctor()) {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->ApplyMasks(refr, isFirstPerson, armor, addon, object, flags, overrides, layer);
		}
	}
	void ManageTints() {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->ManageTints();
		}
	}
	void ReleaseTints() {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->ReleaseTints();
		}
	}
	void Revert() {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->Revert();
		}
	}

	bool IsDyeable(RE::TESObjectARMO* armor) {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->IsDyeable(armor);
		}
	}

	void GetTemplateColorMap(RE::TESObjectREFR* actor, RE::TESObjectARMO* armor, std::map<std::int32_t, std::uint32_t>& colorMap) {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->GetTemplateColorMap(actor, armor, colorMap);
		}
	}
	void GetSlotTextureIndexMap(RE::TESObjectREFR* actor, RE::TESObjectARMO* armor, std::map<std::int32_t, std::uint32_t>& slotTextureIndexMap) {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->GetSlotTextureIndexMap(actor, armor, slotTextureIndexMap);
		}
	}

	void LoadMods() {
		if (TintMaskInterface* tmi = GetTintMaskInterface(); tmi)
		{
			tmi->LoadMods();
		}
	}
};

class FaceMorphInterfaceAPI :
	public NiOverrideAPI
{
	IPluginInterface* faceMorphInterface = nullptr;
	const std::string interfaceName_ = "FaceMorph";
public:
	FaceMorphInterfaceAPI(std::string interfaceName = "FaceMorph") : interfaceName_(interfaceName) {};
	~FaceMorphInterfaceAPI() {};

	inline bool FaceMorphInterfaceload() {
		if (!faceMorphInterface && NiOverrideload())
		{
			faceMorphInterface = GetExchangeMessage().interfaceMap->QueryInterface(interfaceName_.c_str());
		}
		return faceMorphInterface ? true : false;
	}

	inline std::uint32_t GetVersion() {
		if (FaceMorphInterfaceload())
		{
			return faceMorphInterface->GetVersion();
		}
		return 0;
	}

	inline FaceMorphInterfaceSE* GetFaceMorphInterfaceSE() {
		if (FaceMorphInterfaceload())
		{
			if (faceMorphInterface->GetVersion() == 2)
				return static_cast<FaceMorphInterfaceSE*>(faceMorphInterface);
		}
		return nullptr;
	}
	inline FaceMorphInterfaceAE* GetFaceMorphInterfaceAE() {
		if (FaceMorphInterfaceload())
		{
			if (faceMorphInterface->GetVersion() == 2)
				return static_cast<FaceMorphInterfaceAE*>(faceMorphInterface);
		}
		return nullptr;
	}

	inline float GetFaceMorphValueByName(RE::TESNPC* npc, RE::BSFixedString name) {
		if (VersionCheck(1, 6, 640) < 0)
		{
			if (FaceMorphInterfaceSE* fmi = GetFaceMorphInterfaceSE(); fmi)
			{
				return fmi->GetMorphValueByName(npc, name);
			}
		}
		else
		{
			if (FaceMorphInterfaceAE* fmi = GetFaceMorphInterfaceAE(); fmi)
			{
				return fmi->GetMorphValueByName(npc, name);
			}
		}
		return 0.0f;
	}
	inline void SetFaceMorphValueByName(RE::TESNPC* npc, RE::BSFixedString name, float value) {
		if (VersionCheck(1, 6, 640) < 0)
		{
			if (FaceMorphInterfaceSE* fmi = GetFaceMorphInterfaceSE(); fmi)
			{
				fmi->SetMorphValue(npc, name, value);
			}
		}
		else
		{
			if (FaceMorphInterfaceAE* fmi = GetFaceMorphInterfaceAE(); fmi)
			{
				return fmi->SetMorphValue(npc, name, value);
			}
		}
	}
};
