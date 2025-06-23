#pragma once

namespace Mus {
	struct TaskID {
		RE::FormID refrID;
		std::string geometryName;
		std::int64_t taskID;
	};

	class GeometryData {
	public:
		GeometryData() {};
		GeometryData(RE::BSGeometry* a_geo);
		~GeometryData() {};

		void GetGeometryData(RE::BSGeometry* a_geo);
		void UpdateVertexMap_FaceNormals_FaceTangents();
		void RecalculateNormals(float a_smooth);
		void Subdivision(std::uint32_t a_subCount);
		void VertexSmooth(float a_strength, std::uint32_t a_smoothCount);

		RE::BSGraphics::VertexDesc desc;
		bool hasVertices = false;
		bool hasUV = false;
		bool hasNormals = false;
		bool hasTangents = false;
		bool hasBitangents = false;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> vertices;
		concurrency::concurrent_vector<DirectX::XMFLOAT2> uvs;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> normals;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> tangents;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> bitangents;
		concurrency::concurrent_vector<std::uint32_t> indices;

		struct Vec3Hash {
			size_t operator()(const DirectX::XMFLOAT3& v) const {
				size_t hx = std::hash<int>()(int(v.x * 10000));
				size_t hy = std::hash<int>()(int(v.y * 10000));
				size_t hz = std::hash<int>()(int(v.z * 10000));
				return ((hx ^ (hy << 1)) >> 1) ^ (hz << 1);
			}
		};
		struct Vec3Equal {
			bool operator()(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) const {
				const float eps = floatPrecision;
				return fabs(a.x - b.x) < eps && fabs(a.y - b.y) < eps && fabs(a.z - b.z) < eps;
			}
		}; 
		struct VertexKey {
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT2 uv;
			bool operator==(const VertexKey& other) const {
				const float eps = floatPrecision;
				return fabs(pos.x - other.pos.x) < eps &&
					fabs(pos.y - other.pos.y) < eps &&
					fabs(pos.z - other.pos.z) < eps &&
					fabs(uv.x - other.uv.x) < eps &&
					fabs(uv.y - other.uv.y) < eps;
			}
		};
		struct VertexKeyHash {
			size_t operator()(const VertexKey& k) const {
				size_t hx = std::hash<int>()(int(k.pos.x * 10000));
				size_t hy = std::hash<int>()(int(k.pos.y * 10000));
				size_t hz = std::hash<int>()(int(k.pos.z * 10000));
				size_t hu = std::hash<int>()(int(k.uv.x * 10000));
				size_t hv = std::hash<int>()(int(k.uv.y * 10000));
				return (((((hx ^ (hy << 1)) >> 1) ^ (hz << 1)) ^ (hu << 2)) >> 2) ^ (hv << 3);
			}
		};
		
		// UV seam 처리를 위한 position 기반 맵 추가
		struct PositionKey {
			DirectX::XMFLOAT3 pos;
			bool operator==(const PositionKey& other) const {
				const float eps = floatPrecision;
				return fabs(pos.x - other.pos.x) < eps &&
					fabs(pos.y - other.pos.y) < eps &&
					fabs(pos.z - other.pos.z) < eps;
			}
		};
		struct PositionKeyHash {
			size_t operator()(const PositionKey& k) const {
				return std::hash<int>()(int(k.pos.x * 10000)) ^
					(std::hash<int>()(int(k.pos.y * 10000)) << 1) ^
					(std::hash<int>()(int(k.pos.z * 10000)) << 2);
			}
		};
		
		concurrency::concurrent_unordered_map<VertexKey, concurrency::concurrent_vector<size_t>, VertexKeyHash> vertexMap;
		concurrency::concurrent_unordered_map<PositionKey, concurrency::concurrent_vector<size_t>, PositionKeyHash> positionMap;

		struct FaceUV {
			DirectX::XMFLOAT2 uv0, uv1, uv2;
		};
		concurrency::concurrent_vector<FaceUV> faceUVs;

		struct FaceNormal {
			size_t v0, v1, v2;
			DirectX::XMFLOAT3 normal;
		};
		concurrency::concurrent_vector<FaceNormal> faceNormals;
		concurrency::concurrent_vector<concurrency::concurrent_vector<size_t>> vertexToFaceMap;

		struct FaceTangent {
			size_t v0, v1, v2;
			DirectX::XMFLOAT3 tangent;
			DirectX::XMFLOAT3 bitangent;
		};
		concurrency::concurrent_vector<FaceTangent> faceTangents;
	};

	struct BakeData {
		std::string geometryName;
		std::string textureName;
		GeometryData data;
		std::string srcTexturePath;
		std::string maskTexturePath;
	};

	struct TextureInfo {
		RE::FormID actorID;
		RE::FormID armorID;
		std::uint32_t bipedSlot;
		std::string geoName;
		std::uint32_t vertexCount;
	};

	inline RE::NiPointer<RE::NiSkinPartition> GetSkinPartition(RE::BSGeometry* a_geo)
	{
		if (!a_geo)
			return nullptr;
		if (!a_geo->GetGeometryRuntimeData().skinInstance)
			return nullptr;
		RE::NiSkinInstance* skinInstance = a_geo->GetGeometryRuntimeData().skinInstance.get();
		if (!skinInstance->skinPartition)
			return nullptr;
		return skinInstance->skinPartition;
	}
}