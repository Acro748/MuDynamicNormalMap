#include "Store.h"

namespace Mus {
	std::atomic<bool> IsRaceSexMenu = false;
	std::atomic<bool> IsMainMenu = false;
	std::atomic<bool> IsGamePaused = false;

	bool PerformanceCheck = true;
	bool PerformanceCheckAverage = false;
	bool PerformanceCheckConsolePrint = false;
	bool PerformanceCheckTick = false;

	float weldDistance = 0.0001f;
}
