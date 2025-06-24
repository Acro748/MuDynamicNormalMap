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

		void RunDelayTask();
		void RegisterDelayTask(std::string id, std::function<void()> func);
		void RegisterDelayTask(std::string id, std::uint8_t delayTick, std::function<void()> func);
		std::string GetDelayTaskID(RE::FormID refrID, std::uint32_t bipedSlot);

		std::unordered_set<RE::BSGeometry*> GetGeometries(RE::NiAVObject* a_root, std::function<bool(RE::BSGeometry*)> func);
		std::unordered_set<RE::BSGeometry*> GetGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot);
		std::unordered_set<RE::BSGeometry*> GetSkinGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot);
		std::unordered_set<RE::BSGeometry*> GetGeometries(std::string a_fileName);

		void BakeSkinObjectsNormalMap(RE::Actor* a_actor, std::uint32_t bipedSlot);
		bool QBakeObjectNormalMap(RE::Actor* a_actor, std::unordered_set<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot);

		std::int64_t GenerateUniqueID();
		std::uint64_t AttachBakeObjectNormalMapTaskID(TaskID& taskIDsrc);
		void DetachBakeObjectNormalMapTaskID(TaskID taskIDsrc, std::int64_t a_ownID);
		void ReleaseBakeObjectNormalMapTaskID(TaskID taskIDsrc);
		std::uint64_t GetCurrentBakeObjectNormalMapTaskID(TaskID taskIDsrc);

		void InsertCustomBakeNormalMapMaskTexture(RE::FormID id, std::string baseFolder);

		static inline void SetDeferredWorker() {
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			SetThreadAffinityMask(GetCurrentThread(), Config::GetSingleton().GetPriorityCores());
			std::this_thread::yield();
		}
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

		std::string GetBakeNormalMapMaskTexture(std::string a_geometryName, std::uint32_t bipedSlot, std::filesystem::path baseFolder = "Textures\\MuDynamicTextureTool\\BakeNormalMap");
		std::string GetCustomBakeNormalMapMaskTexture(RE::Actor* a_actor, std::string a_geometryName, std::uint32_t bipedSlot);
		std::unordered_map<RE::FormID, std::string> bakeObjectNormalMapMaskTexture;

		std::unordered_map<RE::FormID, std::unordered_map<std::string, std::int64_t>> bakeObjectNormalMapCounter; // ActorID, GeometryName, BakeID
		std::mutex bakeObjectNormalMapCounterLock;
		concurrency::concurrent_unordered_map<RE::FormID, concurrency::concurrent_unordered_map<std::uint32_t, std::string>> lastNormalMap; // ActorID, VertexCount, TextureName>
	};
}