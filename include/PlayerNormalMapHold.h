#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

namespace Mus
{
    class PlayerNormalMapHold
    {
    public:
        using Clock = std::chrono::steady_clock;

        auto Guard() { return std::unique_lock(mutex, std::defer_lock); }
        bool HeldLocked() const { return token != 0; }

        bool DeferLocked(std::uint32_t slots)
        {
            if (!HeldLocked()) return false;
            deferred |= slots;
            return true;
        }

        std::uint64_t TryHold(Clock::time_point now = Clock::now())
        {
            auto lock = Guard();
            if (!lock.try_lock() || HeldLocked() || IsUpdating()) return 0;
            token = ++sequence;
            expires = now + std::chrono::seconds(5);
            return token;
        }

        std::optional<std::uint32_t> Resume(std::uint64_t requested, std::uint32_t slots)
        {
            if (!requested) return std::nullopt;
            const std::lock_guard lock(mutex);
            if (requested != token) return std::nullopt;
            return ClearLocked() | slots;
        }

        std::optional<std::uint32_t> Expire(Clock::time_point now = Clock::now())
        {
            auto lock = Guard();
            if (!lock.try_lock() || !HeldLocked() || now < expires) return std::nullopt;
            return ClearLocked();
        }

        void Reset()
        {
            const std::lock_guard lock(mutex);
            ClearLocked();
        }

        void SetUpdating(bool value) { updating.store(value); }
        bool IsUpdating() const { return updating.load(); }

    private:
        std::uint32_t ClearLocked()
        {
            token = 0;
            const auto slots = deferred;
            deferred = 0;
            return slots;
        }

        std::mutex mutex;
        std::atomic_bool updating{};
        std::uint64_t token{}, sequence{};
        std::uint32_t deferred{};
        Clock::time_point expires{};
    };
}
