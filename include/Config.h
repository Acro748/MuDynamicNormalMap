#pragma once

namespace Mus {
    enum SIMDType {
        noSIMD,
        sse2,
        sse4,
        avx,
        avx2,
        total
    };
    SIMDType GetSIMDType(bool scan = false);

    class Config {
    public:
        [[nodiscard]] static Config& GetSingleton() {
            static Config instance;
            return instance;
        };

        bool LoadLogging();
        bool LoadConfig();
        bool LoadConfig(std::ifstream& configfile);

        enum AutoTaskQList : std::uint8_t {
            Immediately = 0,
            Fastest,
            Faster,
            Balanced,
            BetterPerformance,
            BestPerformance,
            Total
        };

        //Debug
        [[nodiscard]] inline spdlog::level::level_enum GetLogLevel() const noexcept {
            return logLevel;
        }
        [[nodiscard]] inline spdlog::level::level_enum GetFlushLevel() const noexcept {
            return flushLevel;
        }
        [[nodiscard]] inline auto GetDebugTexture() const noexcept {
            return DebugTexture;
        }
        [[nodiscard]] inline auto GetTickTimeToRealFPS() const noexcept {
            return TickTimeToRealFPS;
        }
        [[nodiscard]] inline auto GetQueueTime() const noexcept {
            return QueueTime;
        }
        [[nodiscard]] inline auto GetFullUpdateTime() const noexcept {
            return FullUpdateTime;
        }
        [[nodiscard]] inline auto GetActorVertexHasherTime1() const noexcept {
            return ActorVertexHasherTime1;
        }
        [[nodiscard]] inline auto GetActorVertexHasherTime2() const noexcept {
            return ActorVertexHasherTime2;
        }
        [[nodiscard]] inline auto GetGeometryDataTime() const noexcept {
            return GeometryDataTime;
        }
        [[nodiscard]] inline auto GetUpdateNormalMapTime1() const noexcept {
            return UpdateNormalMapTime1;
        }
        [[nodiscard]] inline auto GetUpdateNormalMapTime2() const noexcept {
            return UpdateNormalMapTime2;
        }
        [[nodiscard]] inline auto GetMergeTime() const noexcept {
            return MergeTime;
        }
        [[nodiscard]] inline auto GetGenerateMipsTime() const noexcept {
            return GenerateMipsTime;
        }
        [[nodiscard]] inline auto GetBleedTextureTime1() const noexcept {
            return BleedTextureTime1;
        }
        [[nodiscard]] inline auto GetBleedTextureTime2() const noexcept {
            return BleedTextureTime2;
        }
        [[nodiscard]] inline auto GetCompressTime() const noexcept {
            return CompressTime;
        }
        [[nodiscard]] inline auto GetTextureCopyTime() const noexcept {
            return TextureCopyTime;
        }

        //General
        [[nodiscard]] inline auto GetPlayerEnable() const noexcept {
            return PlayerEnable;
        }
        [[nodiscard]] inline auto GetNPCEnable() const noexcept {
            return NPCEnable;
        }
        [[nodiscard]] inline auto GetMaleEnable() const noexcept {
            return MaleEnable;
        }
        [[nodiscard]] inline auto GetFemaleEnable() const noexcept {
            return FemaleEnable;
        }
        [[nodiscard]] inline auto GetHeadEnable() const noexcept {
            return HeadEnable;
        }

        [[nodiscard]] inline auto GetHotKey1() const noexcept {
            return HotKey1;
        }
        [[nodiscard]] inline auto GetHotKey2() const noexcept {
            return HotKey2;
        }

        [[nodiscard]] inline auto GetUseConsoleRef() const noexcept {
            return UseConsoleRef;
        }

        [[nodiscard]] inline auto GetRevertNormalMap() const noexcept {
            return RevertNormalMap;
        }
        [[nodiscard]] inline auto GetUpdateDelayTick() const noexcept {
            return UpdateDelayTick;
        }

        [[nodiscard]] inline auto GetApplyOverlay() const noexcept {
            return ApplyOverlay;
        }

        [[nodiscard]] inline auto GetRemoveSkinOverrides() const noexcept {
            return RemoveSkinOverrides;
        }
        [[nodiscard]] inline auto GetRemoveNodeOverrides() const noexcept {
            return RemoveNodeOverrides;
        }

        //Geometry
        [[nodiscard]] inline auto GetWeldDistance() const noexcept {
            return WeldDistance;
        }
        [[nodiscard]] inline auto GetBoundaryWeldDistance() const noexcept {
            return BoundaryWeldDistance;
        }
        [[nodiscard]] inline auto GetNormalSmoothDegree() const noexcept {
            return NormalSmoothDegree;
        }
        [[nodiscard]] inline auto GetAllowInvertNormalSmooth() const noexcept {
            return AllowInvertNormalSmooth;
        }
        [[nodiscard]] inline auto GetSubdivision() const noexcept {
            return Subdivision;
        }
        [[nodiscard]] inline auto GetSubdivisionTriThreshold() const noexcept {
            return SubdivisionTriThreshold;
        }
        [[nodiscard]] inline auto GetVertexSmooth() const noexcept {
            return VertexSmooth;
        }
        [[nodiscard]] inline auto GetVertexSmoothStrength() const noexcept {
            return VertexSmoothStrength;
        }
        [[nodiscard]] inline auto GetVertexSmoothByAngle() const noexcept {
            return VertexSmoothByAngle;
        }
        [[nodiscard]] inline auto GetVertexSmoothByAngleThreshold1() const noexcept {
            return VertexSmoothByAngleThreshold1;
        }
        [[nodiscard]] inline auto GetVertexSmoothByAngleThreshold2() const noexcept {
            return VertexSmoothByAngleThreshold2;
        }

        //Texture
        [[nodiscard]] inline auto GetTextureWidth() const noexcept {
            return TextureWidth;
        }
        [[nodiscard]] inline auto GetTextureHeight() const noexcept {
            return TextureHeight;
        }
        [[nodiscard]] inline auto GetTangentZCorrection() const noexcept {
            return TangentZCorrection;
        }
        [[nodiscard]] inline auto GetDetailStrength() const noexcept {
            return DetailStrength;
        }
        [[nodiscard]] inline auto GetIgnoreMissingNormalMap() const noexcept {
            return IgnoreMissingNormalMap;
        }

        //Performance
        [[nodiscard]] inline auto GetGPUEnable() const noexcept {
            return GPUEnable;
        }
        [[nodiscard]] inline auto GetGPUDeviceIndex() const noexcept {
            return GPUDeviceIndex;
        }
        [[nodiscard]] inline auto GetGPUForceSync() const noexcept {
            return GPUForceSync;
        }
        [[nodiscard]] inline auto GetUpdateDistance() const noexcept {
            return UpdateDistance;
        }
        [[nodiscard]] inline auto GetUpdateDistanceVramSave() const noexcept {
            return UpdateDistanceVramSave;
        }
        [[nodiscard]] inline auto GetUseMipMap() const noexcept {
            return UseMipMap;
        }

        [[nodiscard]] inline auto GetAutoTaskQ() const noexcept {
            return AutoTaskQ;
        }
        [[nodiscard]] inline auto GetTaskQTickMS() const noexcept {
            return TaskQTickMS;
        }
        [[nodiscard]] inline auto GetProcessingInLoading() const noexcept {
            return ProcessingInLoading;
        }
        [[nodiscard]] inline auto GetProcessingInLoadingWithMainGPU() const noexcept {
            return ProcessingInLoadingWithMainGPU;
        }
        [[nodiscard]] inline auto GetTextureCompress() const noexcept {
            return TextureCompress;
        }
        [[nodiscard]] inline auto GetTextureCompressQuality() const noexcept {
            return TextureCompressQuality;
        }
        [[nodiscard]] inline auto GetSIMDtype() const noexcept {
            return SIMDtype;
        }
        [[nodiscard]] inline auto GetDiskCache() const noexcept {
            return DiskCache;
        }
        [[nodiscard]] inline auto GetDiskCacheFolder() const noexcept {
            return DiskCacheFolder;
        }
        [[nodiscard]] inline auto GetDiskCacheLimitMB() const noexcept {
            return DiskCacheLimitMB;
        }
        [[nodiscard]] inline auto GetClearDiskCache() const noexcept {
            return ClearDiskCache;
        }

        //RealtimeDetect
        [[nodiscard]] inline auto GetRealtimeDetect() const noexcept {
            return RealtimeDetect;
        }
        [[nodiscard]] inline auto GetRealtimeDetectHead() const noexcept {
            return RealtimeDetectHead;
        }
        [[nodiscard]] inline auto GetRealtimeDetectOnBackGround() const noexcept {
            return RealtimeDetectOnBackGround;
        }
        [[nodiscard]] inline auto GetDetectDistance() const noexcept {
            return DetectDistance;
        }
        [[nodiscard]] inline auto GetDetectTickMS() const noexcept {
            return DetectTickMS;
        }

    protected:
        //Debug
        spdlog::level::level_enum logLevel{ spdlog::level::level_enum::info };
        spdlog::level::level_enum flushLevel{ spdlog::level::level_enum::trace };
        bool DebugTexture = false;
        bool TickTimeToRealFPS = false;
        bool QueueTime = false;
        bool FullUpdateTime = false;
        bool ActorVertexHasherTime1 = false;
        bool ActorVertexHasherTime2 = false;
        bool GeometryDataTime = false;
        bool UpdateNormalMapTime1 = false;
        bool UpdateNormalMapTime2 = false;
        bool MergeTime = false;
        bool GenerateMipsTime = false;
        bool BleedTextureTime1 = false;
        bool BleedTextureTime2 = false;
        bool CompressTime = false;
        bool TextureCopyTime = false;

        //General
        bool PlayerEnable = true;
        bool NPCEnable = true;
        bool MaleEnable = true;
        bool FemaleEnable = true;
        bool HeadEnable = false;

        std::uint32_t HotKey1 = 0;
        std::uint32_t HotKey2 = 43;

        bool UseConsoleRef = true;

        bool RevertNormalMap = false;

        std::uint8_t UpdateDelayTick = 1;

        bool ApplyOverlay = true;

        bool RemoveSkinOverrides = true;
        bool RemoveNodeOverrides = true;

        //Geometry
        float WeldDistance = 0.02f;
        float BoundaryWeldDistance = 0.02f;
        float NormalSmoothDegree = 60.0f;
        bool AllowInvertNormalSmooth = false;
        std::uint8_t Subdivision = 0;
        std::uint32_t SubdivisionTriThreshold = 65535;
        std::uint8_t VertexSmooth = 0;
        float VertexSmoothStrength = 0.5f;
        std::uint8_t VertexSmoothByAngle = 0;
        float VertexSmoothByAngleThreshold1 = 45.0f;
        float VertexSmoothByAngleThreshold2 = 60.0f;

        //Texture
        std::uint32_t TextureWidth = 2048;
        std::uint32_t TextureHeight = 2048;
        bool TangentZCorrection = true;
        float DetailStrength = 0.5f;
        bool IgnoreMissingNormalMap = true;

        //Performance
        bool GPUEnable = true;
        std::int32_t GPUDeviceIndex = -1;
        bool GPUForceSync = false;
        float UpdateDistance = 4096.0f * 4096.0f;
        bool UpdateDistanceVramSave = false;
		bool UseMipMap = true;

        std::uint8_t AutoTaskQ = AutoTaskQList::Balanced;
        bool ProcessingInLoading = false;
        bool ProcessingInLoadingWithMainGPU = true;
        std::clock_t TaskQTickMS = 100;
        std::int8_t TextureCompress = 1; //-1 auto compress, 0 no compress, 1 cpu bc7, 2 gpu bc7
        std::uint8_t TextureCompressQuality = 1;
        std::int8_t SIMDtype = SIMDType::avx;
        bool DiskCache = true;
        std::string DiskCacheFolder = "Data\\SKSE\\Plugins\\MuDynamicNormalMap\\DiskCache";
        std::uint32_t DiskCacheLimitMB = 500;
        bool ClearDiskCache = true;

        //RealtimeDetect
        bool RealtimeDetect = true;
        std::uint8_t RealtimeDetectHead = 1; //0 disable, 1 morph data only, all data
        bool RealtimeDetectOnBackGround = false;
        float DetectDistance = 512.0f * 512.0f;
        std::clock_t DetectTickMS = 1000; //1sec

    public:
        inline std::string getCurrentSettingValue(std::string s)
        {
            ltrim(s, '[');
            rtrim(s, ']');
            return s;
        }

        static inline int GetIntValue(std::string valuestr)
        {
            int value = 0;
            try {
                value = std::stoi(valuestr);
            }
            catch (...) {
                value = 0;
            }
            return value;
        }

        static inline std::uint32_t GetUIntValue(std::string valuestr)
        {
            std::uint32_t value = 0;
            try {
                value = std::stoul(valuestr);
            }
			catch (...) {
				value = 0;
			}
            return value;
        }

        static inline float GetFloatValue(std::string valuestr)
        {
            float value = 0;
            value = strtof(valuestr.c_str(), 0);
            return value;
        }

        static inline bool GetBoolValue(std::string valuestr)
        {
            return !IsSameString(valuestr, "false");
        }

        static inline std::uint8_t GetThirdTypeValue(std::string valuestr) //false, true, auto
        {
            if (IsSameString(valuestr, "False"))
                return 0;
            else if (IsSameString(valuestr, "True"))
                return 1;
            return 2;
        }

        static inline RE::FormID GetFormIDValue(std::string valuestr)
        {
            RE::FormID value;
            value = GetHex(valuestr.c_str());
            return value;
        }

        static inline void skipComments(std::string& str)
        {
            auto pos = str.find("#");
            if (pos != std::string::npos)
            {
                str.erase(pos);
            }
        }

        static inline std::string GetConfigSetting(std::string line, std::string& variable)
        {
            std::string value = "";
            std::vector<std::string> splittedLine = split(line, '=');
            variable = "";
            if (splittedLine.size() > 1)
            {
                variable = splittedLine[0];
                trim(variable);

                std::string valuestr = splittedLine[1];
                trim(valuestr);
                value = valuestr;
            }
            return value;
        }

    protected:
        inline std::vector<RE::FormID> ConfigLineSplitterFormID(std::string valuestr)
        {
            std::vector<std::string> SplittedFormID = split(valuestr, '|');
            std::vector<RE::FormID> value;
            for (size_t index = 0; index < SplittedFormID.size(); index++)
            {
                trim(SplittedFormID[index]);
                value.emplace_back(GetHex(SplittedFormID[index].c_str()));
            }
            return value;
        }
    };

    class MultipleConfig : public Config {
    public:
        bool LoadConditionFile();

        static inline std::vector<std::filesystem::path> GetAllFiles(std::string folder)
        {
            std::vector<std::filesystem::path> files;

            auto path = std::filesystem::path(folder);
            if (!std::filesystem::exists(path))
                return files;
            if (!std::filesystem::is_directory(path))
                return files;

            for (const auto& file : std::filesystem::directory_iterator(folder))
            {
                files.emplace_back(file.path());
            }
            return files;
        }

        static inline std::vector<std::filesystem::path> GetAllFilesWithSub(std::string folder)
        {
            std::vector<std::filesystem::path> files;

            auto path = std::filesystem::path(folder);
            if (!std::filesystem::exists(path))
                return files;
            if (!std::filesystem::is_directory(path))
                return files;

            for (const auto& file : std::filesystem::recursive_directory_iterator(folder))
            {
                files.emplace_back(file.path());
            }
            return files;
        }

    public:
        static inline bool isDiffuse(std::string filename) {
            return !(isNormal(filename) 
                || isSpecular(filename) 
                || isEnvironmentMask(filename)
                || isSubSurface(filename)
                || isEnvironment(filename)); };
        static inline bool isNormal(std::string filename) { return (IsContainString(filename, "_n.dds") || IsContainString(filename, "_msn.dds")); };
        static inline bool isSpecular(std::string filename) { return IsContainString(filename, "_s.dds"); };
        static inline bool isEnvironmentMask(std::string filename) { return IsContainString(filename, "_m.dds"); };
        static inline bool isSubSurface(std::string filename) { return IsContainString(filename, "_sk.dds"); };
        static inline bool isEnvironment(std::string filename) { return IsContainString(filename, "_e.dds"); };
    };
    void InitialSetting();
    bool SetImmediately(bool a_Immediately);

}
