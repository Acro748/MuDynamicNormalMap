#pragma once

namespace Mus {
	class GeometryData {
	public:
		GeometryData() {};
		GeometryData(RE::BSGeometry* a_geo);
		~GeometryData() {};

		struct GeometryInfo {
			std::string name;
			RE::BSGraphics::VertexDesc desc;
			bool hasVertices = false;
			bool hasUVs = false;
			bool hasNormals = false;
			bool hasTangents = false;
			bool hasBitangents = false;
			std::uint32_t vertexCount;
		};
		struct ObjectInfo {
			GeometryInfo info;
			std::size_t vertexStart;
			std::size_t vertexEnd;
			std::size_t vertexCount() { return info.hasVertices ? vertexEnd - vertexStart : 0; }
			std::size_t uvStart;
			std::size_t uvEnd;
			std::size_t uvCount() { return info.hasUVs ? uvEnd - uvStart : 0; }
			std::size_t normalStart;
			std::size_t normalEnd;
			std::size_t normalCount() { return info.hasNormals ? normalEnd - normalStart : 0; }
			std::size_t tangentStart;
			std::size_t tangentEnd;
			std::size_t tangentCount() { return info.hasTangents ? tangentEnd - tangentStart : 0; }
			std::size_t bitangentStart;
			std::size_t bitangentEnd;
			std::size_t bitangentCount() { return info.hasBitangents ? bitangentEnd - bitangentStart : 0; }
			std::size_t indicesStart;
			std::size_t indicesEnd;
			std::size_t indicesCount() { return indicesEnd - indicesStart; }
			std::vector<std::uint8_t> geometryBlockData;
			std::vector<RE::NiPoint3> dynamicBlockData;
			std::vector<std::uint16_t> indicesBlockData;
		};

		RE::BSFaceGenBaseMorphExtraData* GetMorphExtraData(RE::BSGeometry* a_geometry);

		bool GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info);
		bool CopyGeometryData(RE::BSGeometry* a_geo);
		void GetGeometryData();
		void UpdateMap();
		void RecalculateNormals(float a_smooth);
		void Subdivision(std::uint32_t a_subCount);
		void VertexSmooth(float a_strength, std::uint32_t a_smoothCount);

		GeometryInfo mainInfo;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> vertices;
		concurrency::concurrent_vector<DirectX::XMFLOAT2> uvs;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> normals;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> tangents;
		concurrency::concurrent_vector<DirectX::XMFLOAT3> bitangents;
		concurrency::concurrent_vector<std::uint32_t> indices;

		concurrency::concurrent_vector<std::pair<std::string, ObjectInfo>> geometries; //geometry name, ObjectInfo
		std::uint32_t mainGeometryIndex = 0;

		struct VertexKey {
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT2 uv;
			bool operator==(const VertexKey& other) const {
				return fabs(pos.x - other.pos.x) < weldDistance &&
					fabs(pos.y - other.pos.y) < weldDistance &&
					fabs(pos.z - other.pos.z) < weldDistance &&
					fabs(uv.x - other.uv.x) < weldDistance &&
					fabs(uv.y - other.uv.y) < weldDistance;
			}
		};
		struct VertexKeyHash {
			std::size_t operator()(const VertexKey& k) const {
				std::size_t hx = std::hash<std::int32_t>()(std::int32_t(k.pos.x * weldDistanceMult));
				std::size_t hy = std::hash<std::int32_t>()(std::int32_t(k.pos.y * weldDistanceMult));
				std::size_t hz = std::hash<std::int32_t>()(std::int32_t(k.pos.z * weldDistanceMult));
				std::size_t hu = std::hash<std::int32_t>()(std::int32_t(k.uv.x * weldDistanceMult));
				std::size_t hv = std::hash<std::int32_t>()(std::int32_t(k.uv.y * weldDistanceMult));
				return (((((hx ^ (hy << 1)) >> 1) ^ (hz << 1)) ^ (hu << 2)) >> 2) ^ (hv << 3);
			}
		};
		//including uv seam
		concurrency::concurrent_unordered_map<VertexKey, concurrency::concurrent_vector<std::uint32_t>, VertexKeyHash> vertexMap;

		struct PositionKey {
			DirectX::XMFLOAT3 pos;
			bool operator==(const PositionKey& other) const {
				return fabs(pos.x - other.pos.x) < weldDistance &&
					fabs(pos.y - other.pos.y) < weldDistance &&
					fabs(pos.z - other.pos.z) < weldDistance;
			}
		};
		struct PositionKeyHash {
			std::size_t operator()(const PositionKey& k) const {
				return std::hash<std::int32_t>()(std::int32_t(k.pos.x * weldDistanceMult)) ^
					(std::hash<std::int32_t>()(std::int32_t(k.pos.y * weldDistanceMult)) << 1) ^
					(std::hash<std::int32_t>()(std::int32_t(k.pos.z * weldDistanceMult)) << 2);
			}
		};
		//without uv seam
		concurrency::concurrent_unordered_map<PositionKey, concurrency::concurrent_vector<std::uint32_t>, PositionKeyHash> positionMap;

		struct FaceNormal {
			std::uint32_t v0, v1, v2;
			DirectX::XMFLOAT3 normal;
		};
		concurrency::concurrent_vector<FaceNormal> faceNormals;
		concurrency::concurrent_vector<concurrency::concurrent_vector<std::uint32_t>> vertexToFaceMap;

		struct FaceTangent {
			std::uint32_t v0, v1, v2;
			DirectX::XMFLOAT3 tangent;
			DirectX::XMFLOAT3 bitangent;
		};
		concurrency::concurrent_vector<FaceTangent> faceTangents;
	};
}
