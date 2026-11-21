#pragma once

#include <cstdint>

namespace MuNormalMapHold
{
    // Send Exchange to "MuDynamicNormalMap" through SKSE messaging after post-load.
    inline constexpr std::uint32_t message = 0x4D444E48;  // MDNH
    inline constexpr std::uint32_t version = 1;
    inline constexpr std::uint64_t notNeeded = UINT64_MAX;

    struct API
    {
        std::uint32_t version;
        // Call on the game thread. Zero means busy; notNeeded means no hold is needed.
        // A successful hold keeps the applied map and expires after five seconds.
        std::uint64_t (*tryHold)(std::uint32_t actor);
        // Release after restoring the mesh. Pass zero slots if there is no lasting change.
        void (*resume)(std::uint64_t token, std::uint32_t refreshSlots);
    };

    struct Exchange
    {
        std::uint32_t version{MuNormalMapHold::version};
        const API* api{};
    };
}
