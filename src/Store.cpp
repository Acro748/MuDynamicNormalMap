#include "Store.h"

namespace Mus {
	std::atomic<bool> IsRaceSexMenu = false;
	std::atomic<bool> IsMainMenu = false;
	std::atomic<bool> IsGamePaused = false;
	std::atomic<bool> IsSaveLoading = false;

	bool PerformanceCheck = true;
	bool PerformanceCheckAverage = false;
	bool PerformanceCheckConsolePrint = false;
	bool PerformanceCheckTick = false;

	std::clock_t currentTime = 0;

	float weldDistance = 0.0001f;
	std::uint32_t weldDistanceMult = 10000;
}
