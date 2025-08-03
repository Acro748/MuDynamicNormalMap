#pragma once

namespace Mus {
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
            Disable = 0,
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
        [[nodiscard]] inline auto GetPerformanceLog() const noexcept {
            return PerformanceLog;
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

        [[nodiscard]] inline auto GetRemoveBeforeNormalMap() const noexcept {
            return RemoveBeforeNormalMap;
        }

        [[nodiscard]] inline auto GetUpdateDistance() const noexcept {
            return UpdateDistance;
        }

        [[nodiscard]] inline auto GetGPUEnable() const noexcept {
            return GPUEnable;
        }
        [[nodiscard]] inline auto GetWaitForRendererTickMS() const noexcept {
            return WaitForRendererTickMS;
        }

        [[nodiscard]] inline auto GetPriorityCores() const noexcept {
            return PriorityCoreMask;
        }
        [[nodiscard]] inline auto GetPriorityCoreCount() const noexcept {
            return PriorityCoreCount;
        }
        [[nodiscard]] inline auto GetAutoTaskQ() const noexcept {
            return AutoTaskQ;
        }
        [[nodiscard]] inline auto GetTaskQMax() const noexcept {
            return TaskQMax;
        }
        [[nodiscard]] inline auto GetTaskQmsTick() const noexcept {
            return TaskQTickMS;
        }
        [[nodiscard]] inline auto GetDirectTaskQ() const noexcept {
            return DirectTaskQ;
        }
        [[nodiscard]] inline auto GetDivideTaskQ() const noexcept {
            return DivideTaskQ;
        }
        [[nodiscard]] inline auto GetUpdateDelayTick() const noexcept {
            return UpdateDelayTick;
        }
        [[nodiscard]] inline auto GetHotKey1() const noexcept {
            return HotKey1;
        }
        [[nodiscard]] inline auto GetHotKey2() const noexcept {
            return HotKey2;
        }
        [[nodiscard]] inline auto GetWeldDistance() const noexcept {
            return WeldDistance;
        }
        [[nodiscard]] inline auto GetNormalSmoothDegree() const noexcept {
            return NormalSmoothDegree;
        }
        [[nodiscard]] inline auto GetSubdivision() const noexcept {
            return Subdivision;
        }
        [[nodiscard]] inline auto GetVertexSmooth() const noexcept {
            return VertexSmooth;
        }
        [[nodiscard]] inline auto GetVertexSmoothStrength() const noexcept {
            return VertexSmoothStrength;
        }
        [[nodiscard]] inline auto GetDetailStrength() const noexcept {
            return DetailStrength;
        }

        [[nodiscard]] inline auto GetTextureMargin() const noexcept {
            return TextureMargin;
        }
        [[nodiscard]] inline auto GetTextureMarginGPU() const noexcept {
            return TextureMarginGPU;
        }
        [[nodiscard]] inline auto GetMergeTextureGPU() const noexcept {
            return MergeTextureGPU;
        }
        [[nodiscard]] inline auto GetTextureCompress() const noexcept {
            return TextureCompress;
        }

        [[nodiscard]] inline auto GetTextureWidth() const noexcept {
            return TextureWidth;
        }
        [[nodiscard]] inline auto GetTextureHeight() const noexcept {
            return TextureHeight;
        }

        [[nodiscard]] inline auto GetBlueRadius() const noexcept {
            return BlueRadius;
        }

        [[nodiscard]] inline auto GetTangentZCorrection() const noexcept {
            return TangentZCorrection;
        }

        [[nodiscard]] inline auto GetRemoveSkinOverrides() const noexcept {
            return RemoveSkinOverrides;
        }
        [[nodiscard]] inline auto GetRemoveNodeOverrides() const noexcept {
            return RemoveNodeOverrides;
		}



        inline void SetTaskQMax(std::uint8_t newTaskQMax) {
            TaskQMax = newTaskQMax;
            logger::info("Set TaskQMax {}", newTaskQMax);
        }
        inline void SetTaskQTickMS(std::uint8_t newTaskQTick) {
            TaskQTickMS = newTaskQTick;
            logger::info("Set TaskQTickMS {}", newTaskQTick);
        }
        inline void SetDirectTaskQ(bool isEnable) {
            DirectTaskQ = isEnable;
            logger::info("DirectTaskQ {}", isEnable ? "Enabled" : "Disabled");
        }
        inline void SetDivideTaskQ(std::uint8_t newDivideTaskQ) {
            DivideTaskQ = newDivideTaskQ;
            logger::info("Set DivideTaskQ {}", newDivideTaskQ);
        }
    protected:
        //Debug
        spdlog::level::level_enum logLevel{ spdlog::level::level_enum::info };
        spdlog::level::level_enum flushLevel{ spdlog::level::level_enum::trace };
        bool DebugTexture = false;
        bool PerformanceLog = false;

        //General
        bool PlayerEnable = true;
        bool NPCEnable = true;
        bool MaleEnable = true;
        bool FemaleEnable = true;
        bool HeadEnable = false;

        bool RealtimeDetect = true;
        std::uint8_t RealtimeDetectHead = 1; //0 disable, 1 morph data only, all data
        bool RealtimeDetectOnBackGround = false;
        float DetectDistance = 512.0f * 512.0f;
        std::clock_t DetectTickMS = 1000; //1sec

        bool RemoveBeforeNormalMap = false;
        float UpdateDistance = 512.0f * 512.0f;

        bool GPUEnable = true;
        std::clock_t WaitForRendererTickMS = 1000; //1sec

        std::unordered_set<std::int32_t> PriorityCoreList;
        unsigned long PriorityCoreMask = 0;
        std::int32_t DetectPriorityCores = 0;
        unsigned long PriorityCoreCount = 0;

        std::uint8_t AutoTaskQ = AutoTaskQList::Disable;
        std::uint8_t TaskQMax = 1;
        std::clock_t TaskQTickMS = 13;
        bool DirectTaskQ = false;
        std::uint8_t DivideTaskQ = 1;

        std::uint8_t UpdateDelayTick = 1;

        std::uint32_t HotKey1 = 0;
        std::uint32_t HotKey2 = 43;

        float WeldDistance = 0.02f;
        float NormalSmoothDegree = 60.0f;
        std::uint8_t Subdivision = 0;
        std::uint8_t VertexSmooth = 0;
        float VertexSmoothStrength = 0.5f;
        float DetailStrength = 0.5f;

        std::int32_t TextureMargin = -1;
        bool TextureMarginGPU = true;
        bool MergeTextureGPU = true;
        std::int8_t TextureCompress = -1; //-1 no compress, 0 dxt5, 1 bc7

        std::uint32_t TextureWidth = 2048;
        std::uint32_t TextureHeight = 2048;

        bool TangentZCorrection = true;

        std::uint32_t BlueRadius = 8;

        bool RemoveSkinOverrides = true;
        bool RemoveNodeOverrides = true;

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

        void SetPriorityCores(std::int32_t DetectPriorityCores);

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
}
