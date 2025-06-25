#include "HookEvent.h"

namespace Mus {
	void EventHandler::Register(bool dataLoaded)
	{
		if (!dataLoaded)
		{

		}
		else
		{
			if (auto inputManager = RE::BSInputDeviceManager::GetSingleton(); inputManager)
			{
				inputManager->AddEventSink<RE::InputEvent*>(this);
			}
		}
	}

	EventResult EventHandler::ProcessEvent(RE::InputEvent* const* evn, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!evn || !*evn)
			return EventResult::kContinue;

		for (RE::InputEvent* input = *evn; input != nullptr; input = input->next)
		{
			if (input->eventType.all(RE::INPUT_EVENT_TYPE::kButton))
			{
				RE::ButtonEvent* button = input->AsButtonEvent();
				if (!button)
					continue;

				using namespace InputManager;
				std::uint32_t keyCode = 0;
				std::uint32_t keyMask = button->idCode;
				//logger::info("{}", keyMask);
				if (button->device.all(RE::INPUT_DEVICE::kMouse))
					keyCode = InputMap::kMacro_MouseButtonOffset + keyMask;
				else if (//isUsingMotionControllers() &&
						 REL::Module::IsVR() &&
						 button->device.underlying() >= INPUT_DEVICE_CROSS_VR::kVirtualKeyboard &&
						 button->device.underlying() <= INPUT_DEVICE_CROSS_VR::kDeviceType_WindowsMRSecondary) {
					keyCode = GetDeviceOffsetForDevice(button->device.underlying()) + keyMask;
				}
				else if (button->device.all(RE::INPUT_DEVICE::kGamepad))
					keyCode = InputMap::GamepadMaskToKeycode(keyMask);
				else
					keyCode = keyMask;

				if (!REL::Module::IsVR())
				{
					if (keyCode >= InputMap::kMaxMacros)
						continue;
				}
				else
				{
					if (keyCode >= InputMap::kMaxMacrosVR)
						continue;
				}

				if (keyCode == Config::GetSingleton().GetBakeKey1())
				{
					isPressedBakeKey1 = button->IsPressed();
				}
				else if (keyCode == Config::GetSingleton().GetBakeKey2())
				{
					if (isPressedBakeKey1 || Config::GetSingleton().GetBakeKey1() == 0)
					{
						RE::Actor* target = nullptr;
						if (auto crossHair = RE::CrosshairPickData::GetSingleton(); crossHair && crossHair->targetActor)
						{
#ifndef ENABLE_SKYRIM_VR
							target = skyrim_cast<RE::Actor*>(crossHair->targetActor.get().get());
#else
							for (std::uint32_t i = 0; i < RE::VRControls::VR_DEVICE::kTotal; i++)
							{
								target = skyrim_cast<RE::Actor*>(crossHair->targetActor[i].get().get());
								if (target)
									break;
							}
#endif
						}
						if (!target)
							target = RE::PlayerCharacter::GetSingleton();
						TaskManager::GetSingleton().QBakeSkinObjectsNormalMap(target, RE::BIPED_OBJECT::kBody);
					}
				}
			}
		}
		return EventResult::kContinue;
	}
}
