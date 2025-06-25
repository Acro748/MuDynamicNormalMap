#include "InputManager.h"

namespace InputManager 
{
	namespace InputMap
	{
		std::uint32_t GamepadMaskToKeycode(std::uint32_t keyMask)
		{
			switch (keyMask)
			{
			case REX::W32::XINPUT_GAMEPAD_DPAD_UP:			return kGamepadButtonOffset_DPAD_UP;
			case REX::W32::XINPUT_GAMEPAD_DPAD_DOWN:		return kGamepadButtonOffset_DPAD_DOWN;
			case REX::W32::XINPUT_GAMEPAD_DPAD_LEFT:		return kGamepadButtonOffset_DPAD_LEFT;
			case REX::W32::XINPUT_GAMEPAD_DPAD_RIGHT:		return kGamepadButtonOffset_DPAD_RIGHT;
			case REX::W32::XINPUT_GAMEPAD_START:			return kGamepadButtonOffset_START;
			case REX::W32::XINPUT_GAMEPAD_BACK:				return kGamepadButtonOffset_BACK;
			case REX::W32::XINPUT_GAMEPAD_LEFT_THUMB:		return kGamepadButtonOffset_LEFT_THUMB;
			case REX::W32::XINPUT_GAMEPAD_RIGHT_THUMB:		return kGamepadButtonOffset_RIGHT_THUMB;
			case REX::W32::XINPUT_GAMEPAD_LEFT_SHOULDER:	return kGamepadButtonOffset_LEFT_SHOULDER;
			case REX::W32::XINPUT_GAMEPAD_RIGHT_SHOULDER:	return kGamepadButtonOffset_RIGHT_SHOULDER;
			case REX::W32::XINPUT_GAMEPAD_A:				return kGamepadButtonOffset_A;
			case REX::W32::XINPUT_GAMEPAD_B:				return kGamepadButtonOffset_B;
			case REX::W32::XINPUT_GAMEPAD_X:				return kGamepadButtonOffset_X;
			case REX::W32::XINPUT_GAMEPAD_Y:				return kGamepadButtonOffset_Y;
			case 0x9:										return kGamepadButtonOffset_LT;
			case 0xA:										return kGamepadButtonOffset_RT;
			default:										return kMaxMacros; // Invalid
			}
		}
	}

	bool isUsingMotionControllers()
	{
		if (!REL::Module::IsVR())
			return false;
		return static_cast<bool>(REL::RelocationID(0x01E717A8, 0x0).address());
	}

	std::uint32_t GetDeviceOffsetForDevice(std::uint32_t deviceType)
	{
		switch (deviceType) {
		case kVRRight:
			return InputMap::kMacro_OculusPrimaryOffset;
			break;
		case kVRLeft:
			return InputMap::kMacro_OculusSecondaryOffset;
			break;
		case kDeviceType_WindowsMRPrimary:
			return InputMap::kMacro_WindowsMRPrimaryOffset;
			break;
		case kDeviceType_WindowsMRSecondary:
			return InputMap::kMacro_WindowsMRSecondaryOffset;
			break;
		case kGamepad:
			return InputMap::kMacro_GamepadOffset;
			break;
		case kMouse:
			return InputMap::kMacro_MouseButtonOffset;
			break;
		case kKeyboard:
			return InputMap::kMacro_KeyboardOffset;
			break;
		default:
			return InputMap::kMacro_VivePrimaryOffset;
			break;
		}
	}
}