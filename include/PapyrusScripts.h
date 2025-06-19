#pragma once

namespace Mus {
    namespace Papyrus {
        constexpr std::string_view ScriptFileName = "MuDynamicTextureTool";
        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);
    }
}
