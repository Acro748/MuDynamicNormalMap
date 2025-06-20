#include "Store.h"

namespace Mus {
	std::atomic<bool> IsRaceSexMenu = false;
	std::atomic<bool> IsMainMenu = false;
	std::atomic<bool> IsGamePaused = false;

	bool PerformanceCheck = false;
	bool PerformanceCheckAverage = false;
	bool PerformanceCheckConsolePrint = false;
}
