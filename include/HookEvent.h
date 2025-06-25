#pragma once

namespace Mus {
	using EventResult = RE::BSEventNotifyControl;

	class EventHandler final : 
		public RE::BSTEventSink<RE::InputEvent*> {

	public:
		static EventHandler& GetSingleton() {
			static EventHandler instance;
			return instance;
		}

		void Register(bool dataLoaded);
	protected:
		EventResult ProcessEvent(RE::InputEvent* const* evn, RE::BSTEventSource<RE::InputEvent*>*) override;

	private:
		bool isPressedBakeKey1 = false;
	};
}