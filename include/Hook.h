#pragma once
#include "detours/detours.h"

namespace Mus {
	template <class Event = void>
	class IEventListener
	{
	public:
		virtual void onEvent(const Event&) = 0;
	};

	template <class Event = void>
	class IEventDispatcher
	{
	public:

		virtual ~IEventDispatcher()
		{
		}

		virtual void addListener(IEventListener<Event>*) = 0;
		virtual void removeListener(IEventListener<Event>*) = 0;
		virtual void dispatch(const Event&) = 0;
	};

	template <class Event = void>
	class EventDispatcherImpl : public IEventDispatcher<Event>
	{
	public:
		EventDispatcherImpl() {}
		~EventDispatcherImpl() {}

		void addListener(IEventListener<Event>* listener) override {
            std::lock_guard lg(m_lock);
			m_listeners.insert(listener);
			m_cacheDirt = true;
		};
        void removeListener(IEventListener<Event>* listener) override {
            std::lock_guard lg(m_lock);
			m_listeners.erase(listener);
			m_cacheDirt = true;
		};
        void dispatch(const Event& event) override {
            if (m_cacheDirt) {
                std::lock_guard lg(m_lock);
                m_caches.clear();
                for (auto& i : m_listeners)
                    m_caches.emplace_back(i);
                m_cacheDirt = false;
            }

            std::shared_lock sl(m_lock);
            for (auto i : m_caches)
                i->onEvent(event);
        };

	private:
        std::shared_mutex m_lock;
		std::unordered_set<IEventListener<Event>*> m_listeners;
		std::vector<IEventListener<Event>*> m_caches;
		std::atomic<bool> m_cacheDirt = false;
	};

	struct FrameEvent
	{
		bool gamePaused;
	};

	struct QuitGameEvent
	{
	};

	struct FacegenNiNodeEvent
	{
		RE::NiNode* root;
		RE::BSFaceGenNiNode* facegenNiNode;
		bool skinned;
	};

	struct ActorChangeHeadPartEvent
	{
		RE::Actor* actor;
		RE::BGSHeadPart* oldHeadPart;
		RE::BGSHeadPart* newHeadPart;
	};

	struct ArmorAttachEvent
	{
		RE::Actor* actor;
		RE::NiNode* armor;
		RE::NiNode* skeleton;
		RE::NiAVObject* attachedNode;
		std::uint32_t bipedSlot;
		bool hasAttached = false;
	};

	struct PlayerCellChangeEvent
	{
		bool IsExterior = false;
		bool IsChangedInOut = false;
	};

	extern EventDispatcherImpl<FrameEvent>  g_frameEventDispatcher;
	extern EventDispatcherImpl<QuitGameEvent>  g_quitGameEventDispatcher;
	extern EventDispatcherImpl<FacegenNiNodeEvent> g_facegenNiNodeEventDispatcher;
	extern EventDispatcherImpl<ActorChangeHeadPartEvent> g_actorChangeHeadPartEventDispatcher;
	extern EventDispatcherImpl<ArmorAttachEvent> g_armorAttachEventEventDispatcher;
	extern EventDispatcherImpl<PlayerCellChangeEvent> g_playerCellChangeEventDispatcher;

	void hook();
}
