#pragma once

namespace Mus {
    namespace Papyrus {
        constexpr std::string_view ScriptFileName = "MuDynamicNormalMap";
        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);
    }
}
