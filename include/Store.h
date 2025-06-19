#pragma once

namespace Mus {
	const RE::NiPoint3 emptyPoint = RE::NiPoint3(0, 0, 0);
	const RE::NiMatrix3 emptyRotate = RE::NiMatrix3();

	extern std::atomic<bool> IsRaceSexMenu;
	extern std::atomic<bool> IsMainMenu;
	extern std::atomic<bool> IsGamePaused;

	extern bool PerformanceCheck;
	extern bool PerformanceCheckAverage;
	extern bool PerformanceCheckConsolePrint;

	#define MATH_PI       3.14159265358979323846
	constexpr float toDegree = 180 / MATH_PI;
	constexpr float toRadian = MATH_PI / 180;
	constexpr float Scale_havokWorld = 0.0142875f;
	constexpr float Scale_skyrimUnit = 0.046875f;
	constexpr float Scale_skyrimImperial = 0.5625f;
	constexpr float Scale_skyrimMetric = 0.70028f;

	constexpr float TimeTick60 = 1.0f / 60.0f;
	constexpr float TimeTick60msec = TimeTick60 * 1000;
}
