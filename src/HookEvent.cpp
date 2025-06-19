#include "HookEvent.h"

namespace Mus {
	void EventHandler::Register()
	{
		if (const auto Menu = RE::UI::GetSingleton(); Menu) {
			logger::info("Sinking menu events...");
			Menu->AddEventSink<RE::MenuOpenCloseEvent>(this);
		}

		if (const auto EventHolder = RE::ScriptEventSourceHolder::GetSingleton(); EventHolder) {
			logger::info("Sinking load/switch events...");
			EventHolder->AddEventSink<RE::TESLoadGameEvent>(this);
		}

		if (const auto NiNodeEvent = SKSE::GetNiNodeUpdateEventSource(); NiNodeEvent)
		{
			NiNodeEvent->AddEventSink<SKSE::NiNodeUpdateEvent>(this);
		}
	}

	EventResult EventHandler::ProcessEvent(const RE::MenuOpenCloseEvent* evn, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!evn || !evn->menuName.c_str())
			return EventResult::kContinue;

		if (evn->opening)
			MenuOpened(evn->menuName.c_str());
		else
			MenuClosed(evn->menuName.c_str());

		return EventResult::kContinue;
	}

	void EventHandler::MenuOpened(std::string name)
	{
		if (name == "Main Menu")
		{
			logger::info("Detected Main Menu"); 
			IsMainMenu.store(true);
		}
		else if (name == "RaceSex Menu")
		{
			logger::info("Detected RaceSex Menu");
			IsRaceSexMenu.store(true);
		}
	}

	void EventHandler::MenuClosed(std::string name)
	{
		if (name == "Main Menu")
		{
			IsMainMenu.store(false);
		}
		else if (name == "RaceSex Menu")
		{
			IsRaceSexMenu.store(false);
		}
	}

	EventResult EventHandler::ProcessEvent(const RE::TESLoadGameEvent* evn, RE::BSTEventSource<RE::TESLoadGameEvent>*) 
	{
		logger::info("Detected Load Game Event");
		return EventResult::kContinue;
	}

	EventResult EventHandler::ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*)
	{
		if (!evn)
			return EventResult::kContinue;

		auto actor = skyrim_cast<RE::Actor*>(evn->reference);
		if (!actor)
			return EventResult::kContinue;

		return EventResult::kContinue;
	}
}
