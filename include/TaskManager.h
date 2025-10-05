#pragma once

namespace Mus {
	class TaskManager :
		public IEventListener<FrameEvent>,
		public IEventListener<FacegenNiNodeEvent>,
		public IEventListener<ActorChangeHeadPartEvent>,
		public IEventListener<ArmorAttachEvent>,
		public IEventListener<PlayerCellChangeEvent>,
		public RE::BSTEventSink<SKSE::NiNodeUpdateEvent>, 
		public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
		public RE::BSTEventSink<RE::InputEvent*> {
	public:
		TaskManager() {};
		~TaskManager() {};

		[[nodiscard]] static TaskManager& GetSingleton() {
			static TaskManager instance;
			return instance;
		}

		void Init(bool dataLoaded);

		enum BipedObjectSlot : std::uint32_t
		{
			kNone = 0,
			kHead = 1 << 0,
			kHair = 1 << 1,
			kBody = 1 << 2,
			kHands = 1 << 3,
			kForearms = 1 << 4,
			kAmulet = 1 << 5,
			kRing = 1 << 6,
			kFeet = 1 << 7,
			kCalves = 1 << 8,
			kShield = 1 << 9,
			kTail = 1 << 10,
			kLongHair = 1 << 11,
			kCirclet = 1 << 12,
			kEars = 1 << 13,
			kModMouth = 1 << 14,
			kModNeck = 1 << 15,
			kModChestPrimary = 1 << 16,
			kModBack = 1 << 17,
			kModMisc1 = 1 << 18,
			kModPelvisPrimary = 1 << 19,
			kDecapitateHead = 1 << 20,
			kDecapitate = 1 << 21,
			kModPelvisSecondary = 1 << 22,
			kModLegRight = 1 << 23,
			kModLegLeft = 1 << 24,
			kModFaceJewelry = 1 << 25,
			kModChestSecondary = 1 << 26,
			kModShoulder = 1 << 27,
			kModArmLeft = 1 << 28,
			kModArmRight = 1 << 29,
			kModMisc2 = 1 << 30,
			kFX01 = 1 << 31,

			kSkin = kBody + kHands + kFeet,
			kSkinWithHead = kSkin + kHead,
			kSkinWithGenital = kSkin + kModPelvisSecondary,
			kSkinWithHeadAndGenital = kSkinWithHead + kModPelvisSecondary,
			kAll = 0xFFFFFFFF
		};

		void RunDelayTask();
		void RunUpdateQueue();
		void RegisterDelayTask(std::function<void()> func);
		void RegisterDelayTask(std::int16_t delayTick, std::function<void()> func);

		std::unordered_set<RE::BSGeometry*> GetAllGeometries(RE::Actor* a_actor);

		bool QUpdateNormalMap(RE::Actor* a_actor, bSlotbit bipedSlot);

		bool QUpdateNormalMapImpl(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, bSlotbit bipedSlot);
		void QUpdateNormalMapImpl(RE::FormID a_actorID, std::string a_actorName, GeometryDataPtr a_geoData, UpdateSet a_updateSet);

		bool RemoveNormalMap(RE::Actor* a_actor);

		std::int64_t GenerateUniqueID();
	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const FacegenNiNodeEvent& e) override;
		void onEvent(const ActorChangeHeadPartEvent& e) override;
		void onEvent(const ArmorAttachEvent& e) override;
		void onEvent(const PlayerCellChangeEvent& e) override;
		EventResult ProcessEvent(const SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*) override;
		EventResult ProcessEvent(RE::InputEvent* const* evn, RE::BSTEventSource<RE::InputEvent*>*) override;
		EventResult ProcessEvent(const RE::MenuOpenCloseEvent* evn, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

	private:
		std::vector<std::function<void()>> delayTask; //task;
		std::mutex delayTaskLock;

		bool isPressedHotKey1 = false;
		bool isResetTasks = false;

		bool isPressedExportHotkey1 = false;

		const std::string MDNMPrefix = "[MDNM]";
		std::string GetTextureName(RE::Actor* a_actor, bSlot a_bipedSlot, std::string a_texturePath); // ActorID + slot + TexturePath
		bool GetTextureInfo(std::string a_textureName, TextureInfo& a_textureInfo); // ActorID + BipedSlot + TexturePath
		std::string GetOriginalTexturePath(std::string a_textureName);
		bool IsCreatedByMDNM(std::string a_textureName);
		
		std::string GetDetailNormalMapPath(std::string a_normalMapPath);
		std::string GetDetailNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan);
		std::string GetDetailNormalMapPath(RE::Actor* a_actor);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan);
		std::string GetOverlayNormalMapPath(RE::Actor* a_actor);
		std::string GetMaskNormalMapPath(std::string a_normalMapPath);
		std::string GetMaskNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan);
		std::string GetMaskNormalMapPath(RE::Actor* a_actor);

		concurrency::concurrent_unordered_map<RE::FormID, bSlotbit> updateSlotQueue;
		std::shared_mutex updateQueueLock;
		concurrency::concurrent_unordered_map<RE::FormID, bool> isUpdating;

		std::shared_mutex lastNormalMapLock;
		struct SlotTexKey {
			bSlot slot;
			std::string texturePath;
			bool operator==(const SlotTexKey& other) const {
				return slot == other.slot && texturePath == other.texturePath;
			}
		};
		struct SlotTexHash {
			std::size_t operator()(const SlotTexKey k) const {
				std::size_t h1 = std::hash<std::uint32_t>()(k.slot);
				std::size_t h2 = std::hash<std::string>()(k.texturePath);
				return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
			}
		};
		concurrency::concurrent_unordered_map<RE::FormID, concurrency::concurrent_unordered_map<SlotTexKey, std::string, SlotTexHash>> lastNormalMap; // ActorID, SlotTexKey, TextureName
		concurrency::concurrent_unordered_map<RE::FormID, bool> isActiveActors; // ActorID, isActive
	};
}
