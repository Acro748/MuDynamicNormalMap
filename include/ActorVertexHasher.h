#pragma once

namespace Mus {
	class ActorVertexHasher :
		public IEventListener<FrameEvent>,
		public IEventListener<FacegenNiNodeEvent>,
		public IEventListener<ActorChangeHeadPartEvent>,
		public IEventListener<ArmorAttachEvent>,
		public RE::BSTEventSink<SKSE::NiNodeUpdateEvent> {
	public:
		[[nodiscard]] static ActorVertexHasher& GetSingleton() {
			static ActorVertexHasher instance;
			return instance;
		}

		void Init();

		class Hash {
		private:
            XXH3_state_t* state = nullptr;
			void Init() {
				state = XXH3_createState();
				Reset();
			}
		public:
			Hash() {
				Init();
			}
			Hash(RE::BSGeometry* a_geo) {
				Init();
				Update(a_geo);
			}
			~Hash() { XXH3_freeState(state); }
			bool Update(RE::BSGeometry* a_geo);
            void Update(const void* input, std::size_t len) { XXH3_64bits_update(state, input, len); }
            void Reset() { XXH3_64bits_reset(state); }
			std::size_t GetNewHash() { 
				oldHashValue = hashValue;
				hashValue = XXH3_64bits_digest(state);
				return hashValue;
			}
			std::size_t GetHash() const { return hashValue; }
			std::size_t GetOldHash() const { return oldHashValue; }
		private:
			std::size_t hashValue = 0;
			std::size_t oldHashValue = 0;
		};
		typedef std::shared_ptr<Hash> HashPtr;
		typedef concurrency::concurrent_unordered_map<RE::BSGeometry*, HashPtr> GeometryHash;

		struct GeometryHashData {
			GeometryHash hash;
			bool IsDynamicTriShapeAsHead = false;
		};

		bool Register(RE::Actor* a_actor, RE::BSGeometry* a_geo);
	protected:
		void onEvent(const FrameEvent& e) override; 
		void onEvent(const FacegenNiNodeEvent& e) override;
		void onEvent(const ActorChangeHeadPartEvent& e) override;
		void onEvent(const ArmorAttachEvent& e) override;
		EventResult ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*) override;

	private:
		void CheckingActorHash();
		bool GetHash(RE::Actor* a_actor, GeometryHash& hashMap);

		std::atomic<bool> isDetecting = false;

		mutable std::shared_mutex blockActorsLock;
		std::unordered_map<RE::FormID, bool> blockActors;
		inline void SetBlocked(RE::FormID id, bool blocked) {
			blockActorsLock.lock();
			blockActors[id] = blocked;
			blockActorsLock.unlock();
		}
		inline bool IsBlocked(RE::FormID id) const {
			blockActorsLock.lock_shared();
			auto found = blockActors.find(id);
			bool isBlocked = found != blockActors.end() ? found->second : false;
			blockActorsLock.unlock_shared();
			return isBlocked;
		}

		std::shared_mutex actorHashLock;
		concurrency::concurrent_unordered_map<RE::FormID, GeometryHashData> actorHash;
	};
}