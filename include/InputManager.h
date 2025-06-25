#pragma once

namespace InputManager
{
	namespace InputMap
	{
		enum
		{
			// first 256 for keyboard, then 8 mouse buttons, then mouse wheel up, wheel down, then 16 gamepad buttons
			kMacro_KeyboardOffset = 0,		// not actually used, just for self-documentation
			kMacro_NumKeyboardKeys = 256,

			kMacro_MouseButtonOffset = kMacro_NumKeyboardKeys,	// 256
			kMacro_NumMouseButtons = 8,

			kMacro_MouseWheelOffset = kMacro_MouseButtonOffset + kMacro_NumMouseButtons,	// 264
			kMacro_MouseWheelDirections = 2,

			kMacro_GamepadOffset = kMacro_MouseWheelOffset + kMacro_MouseWheelDirections,	// 266
			kMacro_NumGamepadButtons = 16,

			kMacro_VivePrimaryOffset = kMacro_GamepadOffset + kMacro_NumGamepadButtons, // 282
			kMacro_VivePrimaryButtons = 64, // openvr says max buttons is 64
			kMacro_ViveSecondaryOffset = kMacro_VivePrimaryOffset + kMacro_VivePrimaryButtons, // 346
			kMacro_ViveSecondaryButtons = 64,

			kMacro_OculusPrimaryOffset = kMacro_ViveSecondaryOffset + kMacro_ViveSecondaryButtons, // 410
			kMacro_OculusPrimaryButtons = 64, // openvr says max buttons is 64
			kMacro_OculusSecondaryOffset = kMacro_OculusPrimaryOffset + kMacro_OculusPrimaryButtons, // 474
			kMacro_OculusSecondaryButtons = 64,

			kMacro_WindowsMRPrimaryOffset = kMacro_OculusSecondaryOffset + kMacro_OculusSecondaryButtons, // 538
			kMacro_WindowsMRPrimaryButtons = 64, // openvr says max buttons is 64
			kMacro_WindowsMRSecondaryOffset = kMacro_WindowsMRPrimaryOffset + kMacro_WindowsMRPrimaryButtons, // 602
			kMacro_WindowsMRSecondaryButtons = 64,

			kMaxMacros = kMacro_GamepadOffset + kMacro_NumGamepadButtons,
			kMaxMacrosVR = kMacro_WindowsMRSecondaryOffset + kMacro_WindowsMRSecondaryButtons	// 666
		};

		enum
		{
			kGamepadButtonOffset_DPAD_UP = kMacro_GamepadOffset,	// 266
			kGamepadButtonOffset_DPAD_DOWN,
			kGamepadButtonOffset_DPAD_LEFT,
			kGamepadButtonOffset_DPAD_RIGHT,
			kGamepadButtonOffset_START,
			kGamepadButtonOffset_BACK,
			kGamepadButtonOffset_LEFT_THUMB,
			kGamepadButtonOffset_RIGHT_THUMB,
			kGamepadButtonOffset_LEFT_SHOULDER,
			kGamepadButtonOffset_RIGHT_SHOULDER,
			kGamepadButtonOffset_A,
			kGamepadButtonOffset_B,
			kGamepadButtonOffset_X,
			kGamepadButtonOffset_Y,
			kGamepadButtonOffset_LT,
			kGamepadButtonOffset_RT	// 281
		};

		std::uint32_t GamepadMaskToKeycode(std::uint32_t keyMask);
		//std::uint32_t GamepadKeycodeToMask(std::uint32_t keyCode);
	}

	enum INPUT_DEVICE_CROSS_VR
	{
		kNone = static_cast<std::underlying_type_t<INPUT_DEVICE_CROSS_VR>>(-1),
		kKeyboard = 0,
		kMouse,
		kGamepad,
		kVirtualKeyboard,
		kVRRight = 5,
		kVRLeft = 6,
		kDeviceType_WindowsMRPrimary,
		kDeviceType_WindowsMRSecondary,

		kTotal
	};
	bool isUsingMotionControllers();
	std::uint32_t GetDeviceOffsetForDevice(std::uint32_t deviceType);
}