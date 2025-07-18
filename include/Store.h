#pragma once

namespace Mus {
	const RE::NiPoint3 emptyPoint = RE::NiPoint3(0, 0, 0);
	const RE::NiMatrix3 emptyRotate = RE::NiMatrix3();
	const DirectX::XMVECTOR emptyVector = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	extern std::atomic<bool> IsRaceSexMenu;
	extern std::atomic<bool> IsMainMenu;
	extern std::atomic<bool> IsGamePaused;
	extern std::atomic<bool> IsSaveLoading;

	extern bool PerformanceCheck;
	extern bool PerformanceCheckAverage;
	extern bool PerformanceCheckConsolePrint;
	extern bool PerformanceCheckTick;

	constexpr std::string_view tempTexture = "Textures\\TextureManager\\Temp.dds";

	#define MATH_PI       3.14159265358979323846
	constexpr float toDegree = 180 / MATH_PI;
	constexpr float toRadian = MATH_PI / 180;
	constexpr float Scale_havokWorld = 0.0142875f;
	constexpr float Scale_skyrimUnit = 0.046875f;
	constexpr float Scale_skyrimImperial = 0.5625f;
	constexpr float Scale_skyrimMetric = 0.70028f;

	constexpr float TimeTick60 = 1.0f / 60.0f;
	constexpr float TimeTick60msec = TimeTick60 * 1000;

	constexpr float TaskQTickBase = 13.0f;

	constexpr float floatPrecision = 1e-6f;
	extern float weldDistance;
	extern std::uint32_t weldDistanceMult;

	constexpr std::uint32_t pixelGroup = 2048 * 2048;
}
