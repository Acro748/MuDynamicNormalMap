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

		void QBakeObjectNormalmap(RE::StaticFunctionTag*, RE::Actor* a_actor, std::uint32_t bipedSlot)
		{
			TaskManager::GetSingleton().QBakeObjectNormalMap(a_actor, TaskManager::GetSingleton().GetSkinGeometries(a_actor, bipedSlot), bipedSlot);
		}

        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
            vm->RegisterFunction("GetVersion", ScriptFileName, GetVersion);
            vm->RegisterFunction("GetArmorSlotBit", ScriptFileName, GetArmorSlotBit);
            vm->RegisterFunction("QBakeObjectNormalmap", ScriptFileName, QBakeObjectNormalmap);
            
            return true;
        }
    }
}  // namespace Mus