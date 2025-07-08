#pragma once

namespace Mus {
	class ActorVertexHasher :
		public IEventListener<FrameEvent> {
	public:
		[[nodiscard]] static ActorVertexHasher& GetSingleton() {
			static ActorVertexHasher instance;
			return instance;
		}

		class Hash {
		private:
			XXH64_state_t* state = nullptr;
		public:
			std::size_t hashValue = 0;
			Hash() {
				state = XXH64_createState();
				Reset();
			}
			~Hash() { XXH64_freeState(state); }
			void Update(const void* input, size_t len) { XXH64_update(state, input, len); }
			void Reset() { XXH64_reset(state, 0); }
			std::size_t GetNewHash() { return XXH64_digest(state); }
		};
		typedef std::shared_ptr<Hash> HashPtr;
		typedef concurrency::concurrent_unordered_map<RE::BIPED_OBJECT, HashPtr> GeometryHash;

		bool Register(RE::Actor* a_actor, RE::BIPED_OBJECT bipedSlot);
	protected:
		void onEvent(const FrameEvent& e) override;

	private:
		void CheckingActorHash();
		bool GetHash(RE::Actor* a_actor, GeometryHash& hash);

		concurrency::concurrent_unordered_map<RE::FormID, GeometryHash> ActorHash;
	};
}