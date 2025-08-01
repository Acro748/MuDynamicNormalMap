#include "PapyrusScripts.h"

namespace Mus {
    namespace Papyrus {
		std::uint32_t GetVersion(RE::StaticFunctionTag*)
		{
			auto* plugin = SKSE::PluginDeclaration::GetSingleton();
			auto version = plugin->GetVersion();
			std::string sVersion = std::to_string(static_cast<std::uint32_t>(version.major()));
			sVersion += std::to_string(static_cast<std::uint32_t>(version.minor()));
			sVersion += std::to_string(static_cast<std::uint32_t>(version.patch()));
			std::uint32_t result = 0;
			try {
				result = std::stoul(sVersion);
			}
			catch (...) {
				result = 0;
			}
			return result;
		}

		std::uint32_t GetArmorSlotBit(RE::StaticFunctionTag*, std::uint32_t slot)
		{
			if (slot >= 30)
				slot -= 30;
			return 1 << slot;
		}

		void QUpdateNormalmap(RE::StaticFunctionTag*, RE::Actor* a_actor, std::uint32_t bipedSlot)
		{
			TaskManager::GetSingleton().QUpdateNormalMap(a_actor, bipedSlot);
		}

		concurrency::concurrent_unordered_map<RE::FormID, float> detailStrengthMap;
		void SetDetailStrength(RE::StaticFunctionTag*, RE::Actor* a_actor, float strength)
		{
			if (!a_actor)
				return;
			strength = std::clamp(strength, 0.0f, 1.0f);
			detailStrengthMap[a_actor->formID] = strength;
		}

		concurrency::concurrent_unordered_map<RE::FormID, std::string> normalmaps[normalmapTypes::max];
		int SetNormalMap(RE::StaticFunctionTag*, RE::Actor* a_actor, std::string filePath, int type)
		{
			if (!a_actor || filePath.empty() || type < 0 || type >= normalmapTypes::max)
				return 0;
			if (!IsExistFile(filePath))
				return -1;
			normalmaps[type][a_actor->formID] = filePath;
			return 1;
		}

        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
            vm->RegisterFunction("GetVersion", ScriptFileName, GetVersion);
            vm->RegisterFunction("GetArmorSlotBit", ScriptFileName, GetArmorSlotBit);
            vm->RegisterFunction("QUpdateNormalmap", ScriptFileName, QUpdateNormalmap);
            vm->RegisterFunction("SetDetailStrength", ScriptFileName, SetDetailStrength);
            vm->RegisterFunction("SetNormalMap", ScriptFileName, SetNormalMap);
            
            return true;
        }
    }
}