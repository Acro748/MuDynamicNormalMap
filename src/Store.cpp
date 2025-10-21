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

	std::uint8_t divideTaskQ = 0;
	bool vramSaveMode = true;
	bool isNoSplitGPU = false;

	float weldDistance = 0.0001f;
	float weldDistanceMult = 10000;
	float boundaryWeldDistance = 0.0001f;
	float boundaryWeldDistanceMult = 10000;
	std::chrono::microseconds waitSleepTime = std::chrono::microseconds(1000);
}
