#pragma once

namespace Mus {
	class TaskManager :
		public IEventListener<FrameEvent>,
		public IEventListener<FacegenNiNodeEvent>,
		public IEventListener<ActorChangeHeadPartEvent>,
		public IEventListener<ArmorAttachEvent>,
		public IEventListener<PlayerCellChangeEvent>,
		public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
		public RE::BSTEventSink<SKSE::NiNodeUpdateEvent>, 
		public RE::BSTEventSink<RE::InputEvent*> {
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
		void RunUpdateQueue();
		void RegisterDelayTask(std::function<void()> func);
		void RegisterDelayTask(std::int16_t delayTick, std::function<void()> func);

		typedef std::vector<RE::BSGeometry*> GeometryList;
        GeometryList GetAllGeometries(RE::Actor* a_actor);

		bool QUpdateNormalMap(RE::Actor* a_actor, bSlotbit bipedSlot = BipedObjectSlot::kAll);

		bool QUpdateNormalMapImpl(RE::Actor* a_actor, GeometryList a_srcGeometies, bSlotbit bipedSlot);
		void QUpdateNormalMapImpl(RE::FormID a_actorID, std::string a_actorName, GeometryDataPtr a_geoData, UpdateSet& a_updateSet);

		void RunManageResource(bool isImminently);
		bool RemoveNormalMap(RE::Actor* a_actor);
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

		bool isAfterLoading = false;
        bool isRevertDone = true;

		std::string GetTextureName(RE::Actor* a_actor, bSlot a_bipedSlot, std::string a_texturePath); // ActorID + slot + TexturePath
		bool GetTextureInfo(std::string a_textureName, TextureInfo& a_textureInfo); // ActorID + BipedSlot + TexturePath
		std::string GetOriginalTexturePath(std::string a_textureName);
		
		std::string GetDetailNormalMapPath(std::string a_normalMapPath);
		std::string GetDetailNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan);
		std::string GetDetailNormalMapPath(RE::Actor* a_actor);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan);
		std::string GetOverlayNormalMapPath(RE::Actor* a_actor);
		std::string GetMaskNormalMapPath(std::string a_normalMapPath);
		std::string GetMaskNormalMapPath(std::string a_normalMapPath, std::vector<std::string> a_proxyFolder, bool a_proxyFirstScan);
		std::string GetMaskNormalMapPath(RE::Actor* a_actor);

		std::string FixTexturePath(std::string texturePath);

		typedef std::unordered_map<RE::FormID, bSlotbit> UpdateSlotQueue;
        UpdateSlotQueue updateSlotQueue;
        mutable std::mutex updateSlotQueueLock;

		std::unordered_map<RE::FormID, bool> isActiveActors; // ActorID, isActive
        mutable std::shared_mutex isActiveActorsLock;
        inline void SetIsActiveActor(RE::FormID a_actorID, bool a_isActive) {
            std::lock_guard lg(isActiveActorsLock);
            isActiveActors[a_actorID] = a_isActive;
        }
        inline bool GetIsActiveActor(RE::FormID a_actorID) const {
            std::shared_lock sl(isActiveActorsLock);
            auto it = isActiveActors.find(a_actorID);
            return it != isActiveActors.end() ? it->second : false;
        }

        concurrency::concurrent_unordered_map<RE::FormID, bool> isUpdating;
        inline void SetIsUpdating(RE::FormID a_actorID, bool a_isUpdating) {
            isUpdating[a_actorID] = a_isUpdating;
        }
        inline bool GetIsUpdating(RE::FormID a_actorID) const {
            auto it = isUpdating.find(a_actorID);
            return it != isUpdating.end() ? it->second : false;
        }

		class LastNormalMapData {
            bSlot slot = 0;
            std::string textureName = "";
        public:
            LastNormalMapData() = delete;
            LastNormalMapData(const LastNormalMapData&) = delete;
			LastNormalMapData& operator=(const LastNormalMapData&) = delete;

            LastNormalMapData(const bSlot a_slot, const std::string& a_textureName) : slot(a_slot), textureName(a_textureName) {};
            LastNormalMapData(LastNormalMapData&& other) noexcept : slot(other.slot), textureName(other.textureName) {
                other.textureName = "";
            };
            LastNormalMapData& operator=(LastNormalMapData&& other) noexcept {
                slot = other.slot;
                textureName = other.textureName;
                other.textureName = "";
            };

			bool Is(const bSlot other) const { 
				return slot == other; 
			}
			bool Is(const std::string& other) const { 
				return textureName == other; 
			}
			bool Is(const bSlot otherSlot, const std::string& otherName) const { 
				return Is(otherSlot) && Is(otherName);
			}

			~LastNormalMapData() noexcept { 
				if (!textureName.empty())
					Shader::TextureLoadManager::GetSingleton().ReleaseNiTexture(textureName); 
				logger::debug("Remove texture : {}", textureName);
			};

			bSlot GetSlot() const { return slot; };
			std::string GetTextureName() const { return textureName; };
		};
        typedef std::shared_ptr<LastNormalMapData> LastNormalMapDataPtr;
        typedef std::vector<LastNormalMapDataPtr> LastNormalMapSet;
        std::unordered_map<RE::FormID, LastNormalMapSet> lastNormalMap;
        mutable std::shared_mutex lastNormalMapLock;

		inline bool IsExistLastNormalMap(RE::FormID actorID) const {
            std::lock_guard lg(lastNormalMapLock);
            auto found = lastNormalMap.find(actorID);
            return found != lastNormalMap.end();
        };
		inline bool IsExistLastNormalMap(RE::FormID actorID, bSlot slot) const {
            std::lock_guard lg(lastNormalMapLock);
            auto found = lastNormalMap.find(actorID);
            if (found == lastNormalMap.end()) {
                return false;
            }
            auto it = std::find_if(found->second.cbegin(), found->second.cend(), [&](const LastNormalMapDataPtr& data) {
                return data->Is(slot);
            });
            return it != found->second.cend();
        };
		inline bool IsExistLastNormalMap(RE::FormID actorID, bSlot slot, const std::string& textureName) const {
            std::lock_guard lg(lastNormalMapLock);
            auto found = lastNormalMap.find(actorID);
            if (found == lastNormalMap.end()) {
                return false;
            }
            auto it = std::find_if(found->second.cbegin(), found->second.cend(), [&](const LastNormalMapDataPtr& data) {
                return data->Is(slot, textureName);
            });
            return it != found->second.cend();
        };
		inline void InsertLastNormalMap(RE::FormID actorID, bSlot slot, const std::string& textureName) {
            std::lock_guard lg(lastNormalMapLock);
            auto found = lastNormalMap.find(actorID);
            if (found == lastNormalMap.end()) {
                lastNormalMap[actorID].push_back(std::make_shared<LastNormalMapData>(slot, textureName));
                return;
            }
            auto it = std::find_if(found->second.cbegin(), found->second.cend(), [&](const LastNormalMapDataPtr& data) {
                return data->Is(slot, textureName);
            });
            if (it == found->second.cend())
				lastNormalMap[actorID].push_back(std::make_shared<LastNormalMapData>(slot, textureName));
            return;
        };
        inline void RemoveLastNormalMap(RE::FormID actorID) {
            std::lock_guard lg(lastNormalMapLock);
            lastNormalMap.erase(actorID);
		}
        inline void RemoveLastNormalMap(RE::FormID actorID, bSlot slot) {
            std::lock_guard lg(lastNormalMapLock);
            auto found = lastNormalMap.find(actorID);
            if (found == lastNormalMap.end()) {
                return;
            }
            std::erase_if(found->second, [&](const LastNormalMapDataPtr data) {
                return data->Is(slot);
            });
		}
        inline void RemoveLastNormalMap(RE::FormID actorID, bSlot slot, const std::string& textureName) {
            std::lock_guard lg(lastNormalMapLock);
            auto found = lastNormalMap.find(actorID);
            if (found == lastNormalMap.end()) {
                return;
            }
            std::erase_if(found->second, [&](const LastNormalMapDataPtr data) {
                return data->Is(slot, textureName);
            });
		}

		void ReleaseResourceOnUnloadActors();
	};
}
