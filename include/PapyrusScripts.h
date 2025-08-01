#pragma once

namespace Mus {
    namespace Papyrus {
        constexpr std::string_view ScriptFileName = "MuDynamicNormalMap";
        bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* vm);
		extern concurrency::concurrent_unordered_map<RE::FormID, float> detailStrengthMap;
        enum normalmapTypes {
            detail,
            overlay,
            mask,
            max
        };
        extern concurrency::concurrent_unordered_map<RE::FormID, std::string> normalmaps[normalmapTypes::max];
    }
}
