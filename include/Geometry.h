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
			std::uint32_t vertexCount = 0;
		};
		struct ObjectInfo {
			GeometryInfo info;
			std::size_t vertexStart = 0;
			std::size_t vertexEnd = 0;
			std::size_t vertexCount() const { return vertexEnd - vertexStart; }
			std::size_t uvStart = 0;
			std::size_t uvEnd = 0;
			std::size_t uvCount() const { return uvEnd - uvStart; }
			std::size_t normalStart = 0;
			std::size_t normalEnd = 0;
			std::size_t normalCount() const { return normalEnd - normalStart; }
			std::size_t tangentStart = 0;
			std::size_t tangentEnd = 0;
			std::size_t tangentCount() const { return tangentEnd - tangentStart; }
			std::size_t bitangentStart = 0;
			std::size_t bitangentEnd = 0;
			std::size_t bitangentCount() const { return bitangentEnd - bitangentStart; }
			std::size_t indicesStart = 0;
			std::size_t indicesEnd = 0;
			std::size_t indicesCount() const { return indicesEnd - indicesStart; }
			std::vector<std::uint8_t> geometryBlockData;
			std::vector<RE::NiPoint3> dynamicBlockData1; //without expression
			std::vector<DirectX::XMFLOAT4> dynamicBlockData2; //with expression
			std::vector<std::uint16_t> indicesBlockData;
		};
		static RE::BSFaceGenBaseMorphExtraData* GetMorphExtraData(RE::BSGeometry* a_geometry);
		static std::uint32_t GetVertexCount(RE::BSGeometry* a_geometry);

		bool GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info);
		bool CopyGeometryData(RE::BSGeometry* a_geo);
		void GetGeometryData();
		void UpdateMap();
		void RecalculateNormals(float a_smoothDegree);
		void Subdivision(std::uint32_t a_subCount, std::uint32_t a_triThreshold);
		void VertexSmooth(float a_strength, std::uint32_t a_smoothCount);
		void VertexSmoothByAngle(float a_smoothThreshold1, float a_smoothThreshold2, std::uint32_t a_smoothCount);

		GeometryInfo mainInfo;
		std::vector<DirectX::XMFLOAT3> vertices;
		std::vector<DirectX::XMFLOAT2> uvs;
		std::vector<DirectX::XMFLOAT3> normals;
		std::vector<DirectX::XMFLOAT3> tangents;
		std::vector<DirectX::XMFLOAT3> bitangents;
		std::vector<std::uint32_t> indices;

		struct GeometriesInfo {
			RE::BSGeometry* geometry; //for ptr compare only 
			ObjectInfo objInfo;
			std::uint64_t hash;
		};
		concurrency::concurrent_vector<GeometriesInfo> geometries;
		std::uint32_t mainGeometryIndex = 0;

	private:
		float SmoothStepRange(float x, float A, float B);

		struct BoundaryEdgeKey {
			std::uint32_t v0, v1;
			bool operator==(const BoundaryEdgeKey& other) const {
				return std::min(v0, v1) == std::min(other.v0, other.v1) && std::max(v0, v1) == std::max(other.v0, other.v1);
			}
		};
		struct BoundaryEdgeKeyHash {
			std::size_t operator()(const BoundaryEdgeKey& k) const {
				return (static_cast<std::size_t>(std::min(k.v0, k.v1)) << 32) | std::max(k.v0, k.v1);
			}
		};
		concurrency::concurrent_unordered_map<BoundaryEdgeKey, concurrency::concurrent_vector<std::uint32_t>, BoundaryEdgeKeyHash> boundaryEdgeMap; //edge, face
		std::vector<concurrency::concurrent_vector<BoundaryEdgeKey>> boundaryEdgeVertexMap; //vertex index, edgekey
		inline bool IsBoundaryEdge(const std::uint32_t& v0, const std::uint32_t& v1) {
			const auto it = boundaryEdgeMap.find({ v0, v1 });
			if (it != boundaryEdgeMap.end())
				return it->second.size() == 1;
			return false;
		}
		inline bool IsBoundaryEdge(const BoundaryEdgeKey& k) {
			const auto it = boundaryEdgeMap.find(k);
			if (it != boundaryEdgeMap.end())
				return it->second.size() == 1;
			return false;
		}
		inline bool IsBoundaryVertex(const std::uint32_t& vi) {
			if (boundaryEdgeVertexMap.size() <= vi)
				return false;
			return std::find_if(boundaryEdgeVertexMap[vi].begin(), boundaryEdgeVertexMap[vi].end(), 
								[&](const BoundaryEdgeKey& edgeKey) { return IsBoundaryEdge(edgeKey); }) 
				!= boundaryEdgeVertexMap[vi].end();
		}

		struct VertexKey {
			std::int32_t x, y, z;
			std::int32_t u, v;
			bool operator==(const VertexKey& other) const {
				return x == other.x && y == other.y && z == other.z && 
					u == other.u && v == other.v;
			}
		};
		struct VertexKeyHash {
			std::size_t operator()(const VertexKey& k) const {
				std::size_t hx = std::hash<std::int32_t>()(k.x);
				std::size_t hy = std::hash<std::int32_t>()(k.y);
				std::size_t hz = std::hash<std::int32_t>()(k.z);
				std::size_t hu = std::hash<std::int32_t>()(k.x);
				std::size_t hv = std::hash<std::int32_t>()(k.y);
				return (((((hx ^ (hy << 1)) >> 1) ^ (hz << 1)) ^ (hu << 2)) >> 2) ^ (hv << 3);
			}
		};
		inline VertexKey MakeVertexKey(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& uv) {
			return {
				std::int32_t(std::floor(pos.x * weldDistanceMult)),
				std::int32_t(std::floor(pos.y * weldDistanceMult)),
				std::int32_t(std::floor(pos.z * weldDistanceMult)),
				std::int32_t(std::floor(uv.x * weldDistanceMult)),
				std::int32_t(std::floor(uv.y * weldDistanceMult))
			};
		}
		inline VertexKey MakeBoundaryVertexKey(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& uv) {
			return {
				std::int32_t(std::floor(pos.x * boundaryWeldDistanceMult)),
				std::int32_t(std::floor(pos.y * boundaryWeldDistanceMult)),
				std::int32_t(std::floor(pos.z * boundaryWeldDistanceMult)),
				std::int32_t(std::floor(uv.x * boundaryWeldDistanceMult)),
				std::int32_t(std::floor(uv.y * boundaryWeldDistanceMult))
			};
		}
		//including uv seam
		concurrency::concurrent_unordered_map<VertexKey, concurrency::concurrent_unordered_map<std::uint32_t, bool>, VertexKeyHash> vertexMap; //vertex + uv, face

		struct PositionKey {
			std::int32_t x, y, z;
			bool operator==(const PositionKey& other) const {
				return x == other.x && y == other.y && z == other.z;
			}
		};
		struct PositionKeyHash {
			std::size_t operator()(const PositionKey& k) const {
				return std::hash<std::int32_t>()(k.x) ^
					(std::hash<std::int32_t>()(k.y) << 1) ^
					(std::hash<std::int32_t>()(k.z) << 2);
			}
		};
		inline PositionKey MakePositionKey(const DirectX::XMFLOAT3& pos) {
			return {
				std::int32_t(std::floor(pos.x * weldDistanceMult)),
				std::int32_t(std::floor(pos.y * weldDistanceMult)),
				std::int32_t(std::floor(pos.z * weldDistanceMult))
			};
		}
		inline PositionKey MakeBoundaryPositionKey(const DirectX::XMFLOAT3& pos) {
			return {
				std::int32_t(std::floor(pos.x * boundaryWeldDistanceMult)),
				std::int32_t(std::floor(pos.y * boundaryWeldDistanceMult)),
				std::int32_t(std::floor(pos.z * boundaryWeldDistanceMult))
			};
		}
		//without uv seam
		concurrency::concurrent_unordered_map<PositionKey, concurrency::concurrent_unordered_map<std::uint32_t, bool>, PositionKeyHash> positionMap; //vertex, indices

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

		inline bool IsSameGeometry(const std::uint32_t& v0, const std::uint32_t& v1) {
			const auto it = std::find_if(geometries.begin(), geometries.end(), [&](GeometriesInfo& geoInfo) {
				return geoInfo.objInfo.vertexStart <= v0 && geoInfo.objInfo.vertexEnd > v0;
			});
			if (it == geometries.end())
				return false;
			return it->objInfo.vertexStart <= v1 && it->objInfo.vertexEnd > v1;
		};
	};
	typedef std::shared_ptr<GeometryData> GeometryDataPtr;
}
