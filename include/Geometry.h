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
            std::vector<RE::NiPoint3> dynamicBlockData1;      // without expression
            std::vector<DirectX::XMFLOAT4> dynamicBlockData2; // with expression
            std::vector<std::uint16_t> indicesBlockData;
        };
        static RE::BSFaceGenBaseMorphExtraData* GetMorphExtraData(RE::BSGeometry* a_geometry);
        static std::uint32_t GetVertexCount(RE::BSGeometry* a_geometry);

        bool GetGeometryInfo(RE::BSGeometry* a_geo, GeometryInfo& info);
        bool CopyGeometryData(RE::BSGeometry* a_geo);
        void GetGeometryData();
        void CreateVertexMap();
        void CreateFaceData();
        void RecalculateNormals(float a_smoothDegree);
        void Subdivision(std::uint32_t a_subCount, std::uint32_t a_triThreshold);
        void VertexSmooth(float a_strength, std::uint32_t a_smoothCount);
        void VertexSmoothByAngle(float a_smoothThreshold1, float a_smoothThreshold2, std::uint32_t a_smoothCount);
        void CreateGeometryHash();
        void ApplyNormals();

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
        inline float SmoothStepRange(float x, float A, float B) {
            if (x > A)
                return 0.0f;
            else if (x <= B)
                return 1.0f;
            const float t = (A - x) / (A - B);
            return t * t * (3.0f - 2.0f * t);
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
            bool operator==(const PositionKey& other) const {
                return x == other.x && y == other.y && z == other.z;
            }
            bool operator<(const PositionKey& other) const {
                if (x != other.x)
                    return x < other.x;
                if (y != other.y)
                    return y < other.y;
                return z < other.z;
            }
        };
        struct PositionKeyHash {
            std::uint64_t operator()(const PositionKey& k) const {
                const std::int32_t key[3] = {k.x, k.y, k.z};
                return XXH3_64bits(key, sizeof(key));
            }
        };
        struct PosEntry {
            std::uint64_t key = 0;
            std::uint32_t index = 0;
            bool operator==(const PosEntry& other) const {
                return key == other.key;
            }
            bool operator<(const PosEntry& other) const {
                return key < other.key;
            }
            PosEntry() {};
            PosEntry(const PositionKey& a_key, std::uint32_t a_index) : key(PositionKeyHash()(a_key)), index(a_index) {}
        };
        inline PositionKey MakePositionKey(const DirectX::XMFLOAT3& pos) {
            return {
                std::int32_t(std::floor(pos.x * weldDistanceMult)),
                std::int32_t(std::floor(pos.y * weldDistanceMult)),
                std::int32_t(std::floor(pos.z * weldDistanceMult))};
        }
        inline PositionKey MakeBoundaryPositionKey(const DirectX::XMFLOAT3& pos) {
            return {
                std::int32_t(std::floor(pos.x * boundaryWeldDistanceMult)),
                std::int32_t(std::floor(pos.y * boundaryWeldDistanceMult)),
                std::int32_t(std::floor(pos.z * boundaryWeldDistanceMult))};
        }

        std::vector<std::vector<std::uint32_t>> linkedVertices;

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

        inline bool IsSameGeometry(const std::uint32_t& v0, const std::uint32_t& v1) const {
            const auto it = std::find_if(geometries.cbegin(), geometries.cend(), [&](const GeometriesInfo& geoInfo) {
                return geoInfo.objInfo.vertexStart <= v0 && geoInfo.objInfo.vertexEnd > v0;
            });
            if (it == geometries.cend())
                return false;
            return it->objInfo.vertexStart <= v1 && it->objInfo.vertexEnd > v1;
        };
    };
    typedef std::shared_ptr<GeometryData> GeometryDataPtr;

    template <typename V, typename TP>
    void parallel_sort(V& v, std::shared_ptr<TP> tp) {
        if (v.empty())
            return;
        const std::size_t max = v.size();
        const std::size_t threads = tp->GetThreads() * std::max(1ull, std::min(v.size() / (tp->GetThreads() * 4096), 4ull));
        const std::size_t sub = std::max(1ull, std::min(max, threads));
        const std::size_t unit = (max + sub - 1) / sub;
        std::vector<std::future<void>> processes;
        for (std::size_t t = 0; t < threads; t++) {
            const std::size_t begin = t * unit;
            const std::size_t end = std::min(begin + unit, max);
            processes.push_back(tp->submitAsync([&, t, begin, end]() {
                std::sort(v.begin() + begin, v.begin() + end);
            }));
        }
        for (auto& process : processes) {
            process.get();
        }

        std::size_t current_unit = unit;
        while (current_unit < max) {
            processes.clear();
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