#pragma once

namespace Mus {
	class TaskManager :
		public IEventListener<FrameEvent>,
		public IEventListener<FacegenNiNodeEvent>,
		public IEventListener<ActorChangeHeadPartEvent>,
		public IEventListener<ArmorAttachEvent> {
	public:
		TaskManager() {};
		~TaskManager() {};

		[[nodiscard]] static TaskManager& GetSingleton() {
			static TaskManager instance;
			return instance;
		}

		void Init();

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
		void RegisterDelayTask(std::string id, std::function<void()> func);
		void RegisterDelayTask(std::string id, std::uint8_t delayTick, std::function<void()> func);
		std::string GetDelayTaskID(RE::FormID refrID, std::uint32_t bipedSlot);

		std::unordered_set<RE::BSGeometry*> GetGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot);
		std::unordered_set<RE::BSGeometry*> GetAllGeometries(RE::Actor* a_actor);

		void QUpdateNormalMap(RE::Actor* a_actor, std::uint32_t bipedSlot);
		bool QUpdateNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot);
		bool QUpdateNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::unordered_set<RE::BSGeometry*> a_updateTargets);
		void QUpdateNormalMap(TaskID& taskIDsrc, RE::FormID& id, std::string& actorName, BakeData& bakeData);

		std::int64_t GenerateUniqueID();
		std::uint64_t AttachTaskID(TaskID& taskIDsrc);
		void DetachTaskID(TaskID taskIDsrc, std::int64_t a_ownID);
		void ReleaseTaskID(TaskID taskIDsrc);
		std::uint64_t GetCurrentTaskID(TaskID taskIDsrc);
		bool IsValidTaskID(TaskID taskIDsrc);

	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const FacegenNiNodeEvent& e) override;
		void onEvent(const ActorChangeHeadPartEvent& e) override;
		void onEvent(const ArmorAttachEvent& e) override;

	private:
		std::unordered_map<std::string, std::function<void()>> delayTask; //id, task;
		std::mutex delayTaskLock;

		std::string GetTextureName(RE::Actor* a_actor, std::uint32_t a_bipedSlot, RE::BSGeometry* a_geo); // ActorID + Armor/SkinID + BipedSlot + GeometryName + VertexCount
		bool GetTextureInfo(std::string a_textureName, TextureInfo& a_textureInfo); // ActorID + GeometryName + VertexCount
		
		std::string GetTangentNormalMapPath(std::string a_normalMapPath);
		std::string GetTangentNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder);

		std::unordered_map<RE::FormID, std::unordered_map<std::string, std::int64_t>> updateObjectNormalMapCounter; // ActorID, GeometryName, BakeID
		std::mutex updateObjectNormalMapCounterLock;
		concurrency::concurrent_unordered_map<RE::FormID, concurrency::concurrent_unordered_map<std::uint32_t, std::string>> lastNormalMap; // ActorID, VertexCount, TextureName>
	};
}