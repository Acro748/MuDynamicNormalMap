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

		void QBakeObjectNormalmap(RE::StaticFunctionTag*)
		{

		}

        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
            vm->RegisterFunction("GetVersion", ScriptFileName, GetVersion);
            
            return true;
        }
    }
}  // namespace Mus