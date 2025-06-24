#pragma once

namespace Mus {
	class GeometryData {
	public:
		GeometryData() {};
		GeometryData(RE::BSGeometry* a_geo);
		~GeometryData() {};

		struct GeometryInfo {
			bool hasVertices = false;
			bool hasUVs = false;
			bool hasNormals = false;
			bool hasTangents = false;
			bool hasBitangents = false;
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
		};

		bool GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info);
		bool GetGeometryData(RE::BSGeometry* a_geo);
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
				const float eps = weldDistance;
				return fabs(a.x - b.x) < eps && fabs(a.y - b.y) < eps && fabs(a.z - b.z) < eps;
			}
		};
		struct VertexKey {
			DirectX::XMFLOAT3 pos;
			DirectX::XMFLOAT2 uv;
			bool operator==(const VertexKey& other) const {
				const float eps = weldDistance;
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
				const float eps = weldDistance;
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
}
