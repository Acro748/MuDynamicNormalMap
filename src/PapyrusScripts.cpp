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

		void QBakeObjectNormalmap(RE::StaticFunctionTag*, RE::Actor* a_actor, std::int32_t bipedSlot)
		{
			if (bipedSlot >= 30)
				bipedSlot -= 30;
			if (bipedSlot == -1)
			{
				if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
					return;
				TaskManager::GetSingleton().QBakeObjectNormalMap(a_actor, TaskManager::GetSingleton().GetGeometries(a_actor->loadedData->data3D.get(), [](RE::BSGeometry*) -> bool { return true; }), RE::BIPED_OBJECT::kBody);
			}
			else
			{
				TaskManager::GetSingleton().QBakeObjectNormalMap(a_actor, TaskManager::GetSingleton().GetGeometries(a_actor, bipedSlot), bipedSlot);
			}
		}

        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
            vm->RegisterFunction("GetVersion", ScriptFileName, GetVersion);
            vm->RegisterFunction("QBakeObjectNormalmap", ScriptFileName, QBakeObjectNormalmap);
            
            return true;
        }
    }
}  // namespace Mus