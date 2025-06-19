#pragma once

namespace Mus {
	class TextureManager :
		public IEventListener<FrameEvent>,
		public IEventListener<FacegenNiNodeEvent>,
		public IEventListener<ActorChangeHeadPartEvent>,
		public IEventListener<ArmorAttachEvent> {
	public:
		TextureManager() {};
		~TextureManager() {};

		[[nodiscard]] static TextureManager& GetSingleton() {
			static TextureManager instance;
			return instance;
		}

		void RunDelayTask();
		inline void RegisterDelayTask(std::string id, std::function<void()> func) {
			std::lock_guard<std::mutex> lg(delayTaskLock);
			delayTask[id] = func;
		}
		inline void RegisterDelayTask(std::string id, std::uint8_t delayTick, std::function<void()> func) {
			if (delayTick == 0)
				RegisterDelayTask(id, func);
			else
				RegisterDelayTask(id, --delayTick, func);
		}
		inline std::string GetDelayTaskID(RE::FormID a_refrID, std::uint32_t bipedSlot) {
			return std::to_string(a_refrID) + "_" + std::to_string(bipedSlot);
		}

		std::vector<RE::BSGeometry*> GetGeometries(RE::NiAVObject* a_root, std::string a_geoName = "");
		std::vector<RE::BSGeometry*> GetGeometries(RE::Actor* a_actor, std::uint32_t bipedSlot);
		std::vector<RE::BSGeometry*> GetGeometries(std::string a_fileName);

		struct GeometryData {
			RE::BSGraphics::VertexDesc desc;
			bool hasVertices = false;
			bool hasNormals = false;
			bool hasTangents = false;
			bool hasUV = false;
			concurrency::concurrent_vector<DirectX::XMFLOAT3> vertices;
			concurrency::concurrent_vector<DirectX::XMFLOAT3> normals;
			concurrency::concurrent_vector<DirectX::XMFLOAT2> uvs;
			concurrency::concurrent_vector<DirectX::XMFLOAT3> bitangent;
			concurrency::concurrent_vector<DirectX::XMFLOAT3> tangents;
			concurrency::concurrent_vector<std::uint32_t> indices;
		};
		GeometryData GetGeometryData(RE::BSGeometry* a_geo);
		bool QBakeObjectNormalMap(RE::Actor* a_actor, std::vector<RE::BSGeometry*> a_srcGeometies, std::uint32_t bipedSlot, bool rebake);
		RE::NiPointer<RE::NiSourceTexture> BakeObjectNormalMap(std::string textureName, GeometryData a_data, std::string a_srcTexturePath, std::string a_maskTexturePath, float a_Normalsmooth, std::uint32_t a_subdivision = 0, std::uint32_t a_vertexSmooth = 0, float a_vertexSmoothStrength = 0.0f);


	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const FacegenNiNodeEvent& e) override;
		void onEvent(const ActorChangeHeadPartEvent& e) override;
		void onEvent(const ArmorAttachEvent& e) override;

	private:
		std::unordered_map<std::string, std::function<void()>> delayTask; //id, task;
		std::mutex delayTaskLock;

		inline std::string GetTextureName(RE::Actor* a_actor, RE::TESObjectARMO* a_armor, std::uint32_t a_bipedSlot, RE::BSGeometry* a_geo) { // ActorID + Armor/SkinID + BipedSlot + GeometryName + VertexCount
			if (!a_actor || !a_armor || !a_geo || a_geo->name.empty())
				return "";
			std::uint32_t vertexCount = 0;
			RE::BSTriShape* triShape = a_geo->AsTriShape();
			if (triShape)
				vertexCount = triShape->GetTrishapeRuntimeData().vertexCount;
			auto& runtimeData = a_geo->GetGeometryRuntimeData();
			if (runtimeData.skinInstance && runtimeData.skinInstance->skinPartition)
				vertexCount ? vertexCount : runtimeData.skinInstance->skinPartition->vertexCount;
			return GetHexStr(a_actor->formID) + "_" + GetHexStr(a_armor->formID) + "_" + std::to_string(a_bipedSlot) + "_" + a_geo->name.c_str() + "_" + std::to_string(vertexCount);
		}

		struct TextureInfo {
			RE::FormID actorID;
			RE::FormID armorID;
			std::uint32_t bipedSlot;
			std::string geoName;
			std::uint32_t vertexCount;
		};
		inline bool GetTextureInfo(std::string a_textureName, TextureInfo& a_textureInfo) { // REFR ID + GeometryName + VertexCount
			if (a_textureName.empty())
				return false;
			auto frag = split(a_textureName, '_');
			if (frag.size() < 5)
				return false;
			a_textureInfo.actorID = GetHex(frag[0]);
			a_textureInfo.armorID = GetHex(frag[1]);
			a_textureInfo.bipedSlot = Config::GetUIntValue(frag[2]);
			a_textureInfo.geoName = frag[3];
			a_textureInfo.vertexCount = Config::GetIntValue(frag[4]);
			return true;
		}

		RE::NiPointer<RE::NiSkinPartition> GetSkinPartition(RE::BSGeometry* a_geo);
		void RecalculateNormals(GeometryData& a_data, float a_smooth = 60.0f);
		void Subdivision(GeometryData& a_data, std::uint32_t a_subCount = 1);
		void VertexSmooth(GeometryData& a_data, float a_strength = 0.5f, std::uint32_t a_smoothCount = 1);
		bool ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);

		inline std::int64_t GenerateUniqueID()
		{
			static std::atomic<std::uint64_t> counter{ 0 };
			auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
			return (now << 16) | (counter++ & 0xFFFF); // 시간 + 낮은 16비트 카운터
		}
		inline std::uint64_t AttachBakeObjectNormalMapTaskID(RE::FormID a_refrID, std::uint32_t a_bipedSlot) {
			if (a_refrID == 0 || a_bipedSlot >= RE::BIPED_OBJECT::kTotal)
				return -1;
			std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
			bakeObjectNormalMapCounter[a_refrID][a_bipedSlot] = GenerateUniqueID();
			return bakeObjectNormalMapCounter[a_refrID][a_bipedSlot];
		}
		inline std::uint64_t DetachBakeObjectNormalMapTaskID(RE::FormID a_refrID, std::uint32_t a_bipedSlot, std::int64_t a_ID) {
			if (a_refrID == 0 || a_bipedSlot >= RE::BIPED_OBJECT::kTotal)
				return -1;
			std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
			if (bakeObjectNormalMapCounter[a_refrID][a_bipedSlot] == a_ID)
				bakeObjectNormalMapCounter[a_refrID][a_bipedSlot] = 0;
			return bakeObjectNormalMapCounter[a_refrID][a_bipedSlot];
		}
		inline std::uint64_t ReleaseBakeObjectNormalMapTaskID(RE::FormID a_refrID, std::uint32_t a_bipedSlot) {
			if (a_refrID == 0 || a_bipedSlot >= RE::BIPED_OBJECT::kTotal)
				return -1;
			std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
			bakeObjectNormalMapCounter[a_refrID][a_bipedSlot] = 0;
			return bakeObjectNormalMapCounter[a_refrID][a_bipedSlot];
		}
		inline std::uint64_t GetBakeObjectNormalMapTaskID(RE::FormID a_refrID, std::uint32_t a_bipedSlot) {
			if (a_refrID == 0 || a_bipedSlot >= RE::BIPED_OBJECT::kTotal)
				return -1;
			std::lock_guard<std::mutex> lg(bakeObjectNormalMapCounterLock);
			return bakeObjectNormalMapCounter[a_refrID][a_bipedSlot];
		}
		std::unordered_map<RE::FormID, std::unordered_map<std::uint32_t, std::int64_t>> bakeObjectNormalMapCounter; // <REFR ID, <BipedSlot, BakeID>>
		std::mutex bakeObjectNormalMapCounterLock;
	};
}