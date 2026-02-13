#pragma once

namespace Mus {
    class GeometryData {
    public:
        GeometryData() : tp(currentProcessingThreads.load()) {};
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
            std::uint32_t vertexStart = 0;
            std::uint32_t vertexEnd = 0;
            std::uint32_t vertexCount() const { return vertexEnd - vertexStart; }
            std::uint32_t uvStart = 0;
            std::uint32_t uvEnd = 0;
            std::uint32_t uvCount() const { return uvEnd - uvStart; }
            std::uint32_t normalStart = 0;
            std::uint32_t normalEnd = 0;
            std::uint32_t normalCount() const { return normalEnd - normalStart; }
            std::uint32_t tangentStart = 0;
            std::uint32_t tangentEnd = 0;
            std::uint32_t tangentCount() const { return tangentEnd - tangentStart; }
            std::uint32_t bitangentStart = 0;
            std::uint32_t bitangentEnd = 0;
            std::uint32_t bitangentCount() const { return bitangentEnd - bitangentStart; }
            std::uint32_t indicesStart = 0;
            std::uint32_t indicesEnd = 0;
            std::uint32_t indicesCount() const { return indicesEnd - indicesStart; }
            std::vector<std::uint8_t> geometryBlockData;
            std::vector<RE::NiPoint3> dynamicBlockData1;      // without expression
            std::vector<DirectX::XMFLOAT4> dynamicBlockData2; // with expression
            std::vector<std::uint16_t> indicesBlockData;
        };
        static RE::BSFaceGenBaseMorphExtraData* GetMorphExtraData(RE::BSGeometry* a_geometry);
        static std::uint32_t GetVertexCount(RE::BSGeometry* a_geometry);

        bool GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info);
        bool CopyGeometryData(RE::BSGeometry* a_geo);
        void GetGeometryData();
        void PreProcessing(bool weldAccuracy);
        void CreateFaceData();
        void RecalculateNormals(float a_smoothDegree);
        void Subdivision(std::uint32_t a_subCount, std::uint32_t a_triThreshold, float a_strength, std::uint32_t a_smoothCount, bool weldAccuracy);
        void VertexSmooth(float a_strength, std::uint32_t a_smoothCount);
        void VertexSmoothByAngle(float a_smoothThreshold1, float a_smoothThreshold2, std::uint32_t a_smoothCount);
        void CreateGeometryHash();
        void GeometryProcessing();
        void ApplyNormals();
        bool PrintGeometry(const lString& filePath);

        GeometryInfo mainInfo;
        std::vector<DirectX::XMFLOAT3> vertices;
        std::vector<DirectX::XMFLOAT2> uvs;
        std::vector<DirectX::XMFLOAT3> normals;
        std::vector<DirectX::XMFLOAT3> tangents;
        std::vector<DirectX::XMFLOAT3> bitangents;
        std::vector<std::uint32_t> indices;

        struct GeometriesInfo {
            RE::BSGeometry* geometry; // for ptr compare only
            ObjectInfo objInfo;
            std::uint64_t hash;
        };
        std::vector<GeometriesInfo> geometries;
        std::uint32_t mainGeometryIndex = 0;

    private:
        std::shared_ptr<ThreadPool_ParallelModule> tp;

        inline float SmoothStepRange(float x, float A, float B) const {
            if (x > A)
                return 0.0f;
            else if (x <= B)
                return 1.0f;
            const float t = (A - x) / (A - B);
            return std::clamp(t * t * (3.0f - 2.0f * t), 0.0f, 1.0f);
        }

        struct Edge {
            std::uint32_t v0, v1;
            std::size_t operator()() const {
                return (static_cast<std::size_t>(v0) << 32) | v1;
            }
            bool operator<(const Edge& other) const {
                return (*this)() < other();
            }
            bool operator==(const Edge& other) const {
                return (*this)() == other();
            }
        };

        struct PositionKey {
            std::int32_t x, y, z;
            std::uint64_t operator()() const {
                const std::int32_t key[3] = {x, y, z};
                return XXH3_64bits(key, sizeof(key));
            }
        };
        struct PosEntry {
            std::uint64_t key = 0;
            std::uint32_t index = 0;
            bool operator==(const PosEntry& other) const {
                return key == other.key && index == other.index;
            }
            bool operator<(const PosEntry& other) const {
                return key != other.key ? key < other.key : index < other.index;
            }
            PosEntry() {};
            PosEntry(const PositionKey& a_key, std::uint32_t a_index) : key(a_key()), index(a_index) {}
        };

        inline PositionKey MakeLowPositionKey(const DirectX::XMFLOAT3& pos) const {
            DirectX::XMINT3 p;
            const DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pos);
            const DirectX::XMVECTOR sc = DirectX::XMVectorScale(v, weldDistanceMult);
            const DirectX::XMVECTOR sb = DirectX::XMVectorSubtract(sc, DirectX::XMVectorReplicate(0.25f));
            const DirectX::XMVECTOR f = DirectX::XMVectorFloor(sb);
            DirectX::XMStoreSInt3(&p, f);
            return {p.x, p.y, p.z};
        }
        inline PositionKey MakeHighPositionKey(const DirectX::XMFLOAT3& pos) const {
            DirectX::XMINT3 p;
            const DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pos);
            const DirectX::XMVECTOR sc = DirectX::XMVectorScale(v, weldDistanceMult);
            const DirectX::XMVECTOR ad = DirectX::XMVectorAdd(sc, DirectX::XMVectorReplicate(0.25f));
            const DirectX::XMVECTOR f = DirectX::XMVectorFloor(ad);
            DirectX::XMStoreSInt3(&p, f);
            return {p.x, p.y, p.z};
        }
        inline PositionKey MakePositionKey(const DirectX::XMFLOAT3& pos) const {
            DirectX::XMINT3 p;
            const DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pos);
            const DirectX::XMVECTOR sc = DirectX::XMVectorScale(v, weldDistanceMult);
            const DirectX::XMVECTOR f = DirectX::XMVectorFloor(sc);
            DirectX::XMStoreSInt3(&p, f);
            return {p.x, p.y, p.z};
        }

        inline PositionKey MakeLowBoundaryPositionKey(const DirectX::XMFLOAT3& pos) const {
            DirectX::XMINT3 p;
            const DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pos);
            const DirectX::XMVECTOR sc = DirectX::XMVectorScale(v, boundaryWeldDistanceMult);
            const DirectX::XMVECTOR sb = DirectX::XMVectorSubtract(sc, DirectX::XMVectorReplicate(0.25f));
            const DirectX::XMVECTOR f = DirectX::XMVectorFloor(sb);
            DirectX::XMStoreSInt3(&p, f);
            return {p.x, p.y, p.z};
        }
        inline PositionKey MakeHighBoundaryPositionKey(const DirectX::XMFLOAT3& pos) const {
            DirectX::XMINT3 p;
            const DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pos);
            const DirectX::XMVECTOR sc = DirectX::XMVectorScale(v, boundaryWeldDistanceMult);
            const DirectX::XMVECTOR ad = DirectX::XMVectorAdd(sc, DirectX::XMVectorReplicate(0.25f));
            const DirectX::XMVECTOR f = DirectX::XMVectorFloor(ad);
            DirectX::XMStoreSInt3(&p, f);
            return {p.x, p.y, p.z};
        }
        inline PositionKey MakeBoundaryPositionKey(const DirectX::XMFLOAT3& pos) const {
            DirectX::XMINT3 p;
            const DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pos);
            const DirectX::XMVECTOR sc = DirectX::XMVectorScale(v, boundaryWeldDistanceMult);
            const DirectX::XMVECTOR f = DirectX::XMVectorFloor(sc);
            DirectX::XMStoreSInt3(&p, f);
            return {p.x, p.y, p.z};
        }

        std::vector<std::vector<std::uint32_t>> weldedVertices;
        inline bool IsWeldedVertex(const std::uint32_t vi, const std::uint32_t tv) const {
            return std::find(weldedVertices[vi].cbegin(), weldedVertices[vi].cend(), tv) != weldedVertices[vi].end();
        }
        inline void AddWeldVertices(const std::vector<PosEntry>& entry) {
            if (entry.empty())
                return;
            std::size_t begin = 0;
            for (std::size_t end = 1; end <= entry.size(); end++) {
                if (end != entry.size() && entry[end].key == entry[begin].key)
                    continue;
                for (std::size_t i = begin; i < end; i++) {
                    const std::uint32_t v0 = entry[i].index;
                    weldedVertices[v0].reserve(weldedVertices[v0].size() + end - begin);
                    for (std::size_t j = begin; j < end; j++) {
                        const std::uint32_t v1 = entry[j].index;
                        weldedVertices[v0].push_back(v1);
                    }
                }
                begin = end;
            }
        }
        inline void AddWeldBoundaryVertices(const std::vector<PosEntry>& entry) {
            if (entry.empty())
                return;
            std::size_t begin = 0;
            for (std::size_t end = 1; end <= entry.size(); end++) {
                if (end != entry.size() && entry[end].key == entry[begin].key)
                    continue;
                for (std::size_t i = begin; i < end; i++) {
                    const std::uint32_t v0 = entry[i].index;
                    weldedVertices[v0].reserve(weldedVertices[v0].size() + end - begin);
                    for (std::size_t j = begin; j < end; j++) {
                        const std::uint32_t v1 = entry[j].index;
                        if (IsSameGeometry(v0, v1))
                            continue;
                        weldedVertices[v0].push_back(v1);
                    }
                }
                begin = end;
            }
        }

        struct FaceNormal {
            std::uint32_t v0, v1, v2;
            DirectX::XMFLOAT3 normal;
        };
        std::vector<FaceNormal> faceNormals;
        std::vector<concurrency::concurrent_vector<std::uint32_t>> vertexToFaceMap;

        struct FaceTangent {
            std::uint32_t v0, v1, v2;
            DirectX::XMFLOAT3 tangent;
            DirectX::XMFLOAT3 bitangent;
        };
        std::vector<FaceTangent> faceTangents;

        inline bool IsSameGeometry(const std::uint32_t v0, const std::uint32_t v1) const {
            const auto it = std::find_if(geometries.cbegin(), geometries.cend(), [&](const GeometriesInfo& geoInfo) {
                return geoInfo.objInfo.vertexStart <= v0 && geoInfo.objInfo.vertexEnd > v0;
            });
            if (it == geometries.cend())
                return false;
            return it->objInfo.vertexStart <= v1 && it->objInfo.vertexEnd > v1;
        };

        struct EdgeMid {
            std::uint32_t v0, v1;
            std::uint32_t mv;
            bool operator<(const EdgeMid& other) const {
                return mv < other.mv;
            }
        };
        inline bool IsWeldedEdge(const EdgeMid& e0, const EdgeMid& e1) const {
            return IsWeldedVertex(e0.v0, e1.v0) && IsWeldedVertex(e0.v1, e1.v1);
        }

        struct LocalDate {
            RE::BSGeometry* geometry = nullptr;
            std::vector<DirectX::XMFLOAT3> vertices;
            std::vector<DirectX::XMFLOAT2> uvs;
            std::vector<std::uint32_t> indices;
            ObjectInfo objInfo;
            std::vector<EdgeMid> localCreatedEdges;
        };
    };
    typedef std::shared_ptr<GeometryData> GeometryDataPtr;

    template <typename V, typename TP>
    void parallel_sort(V& v, TP* tp) {
        if (v.empty() || !tp)
            return;
        const std::size_t max = v.size();
        if (max < 4096)
        {
            std::sort(v.begin(), v.end());
            return;
        }
        const std::size_t threads = tp->GetThreads() * std::min(4ull, std::max(1ull, max / 4096));
        const std::size_t sub = std::max(1ull, std::min(max, threads));
        const std::size_t unit = (max + sub - 1) / sub;
        {
            std::vector<std::future<void>> processes;
            for (std::size_t t = 0; t < sub; t++) {
                const std::size_t begin = t * unit;
                const std::size_t end = std::min(begin + unit, max);
                processes.push_back(tp->submitAsync([&, t, begin, end]() {
                    std::sort(v.begin() + begin, v.begin() + end);
                }));
            }
            for (auto& process : processes) {
                process.get();
            }
        }

        std::size_t current_unit = unit;
        while (current_unit < max) {
            std::vector<std::future<void>> processes;
            for (std::size_t i = 0; i < max; i += current_unit * 2) {
                const std::size_t begin = i;
                const std::size_t mid = i + current_unit;
                const std::size_t end = std::min(mid + current_unit, max);
                if (mid >= max)
                    continue;
                processes.push_back(tp->submitAsync([&v, begin, mid, end]() {
                    std::inplace_merge(v.begin() + begin, v.begin() + mid, v.begin() + end);
                }));
            }
            for (auto& process : processes) {
                process.get();
            }
            current_unit *= 2;
        }
    }
} // namespace Mus