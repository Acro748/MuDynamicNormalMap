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

        //General
        [[nodiscard]] inline bool GetPlayerEnable() const noexcept {
            return PlayerEnable;
        }
        [[nodiscard]] inline bool GetNPCEnable() const noexcept {
            return NPCEnable;
        }
        [[nodiscard]] inline bool GetMaleEnable() const noexcept {
            return MaleEnable;
        }
        [[nodiscard]] inline bool GetFemaleEnable() const noexcept {
            return FemaleEnable;
        }
        [[nodiscard]] inline bool GetHeadEnable() const noexcept {
            return HeadEnable;
        }
        [[nodiscard]] inline bool GetRealtimeDetect() const noexcept {
            return RealtimeDetect;
        }
        [[nodiscard]] inline std::uint8_t GetRealtimeDetectHead() const noexcept {
            return RealtimeDetectHead;
        }

        [[nodiscard]] inline bool GetGPUEnable() const noexcept {
            return GPUEnable;
        }
        [[nodiscard]] inline unsigned long GetPriorityCores() const noexcept {
            return PriorityCores;
        }
        [[nodiscard]] inline unsigned long GetPriorityCoreCount() const noexcept {
            return PriorityCoreCount;
        }
        [[nodiscard]] inline std::uint8_t GetAutoTaskQ() const noexcept {
            return AutoTaskQ;
        }
        [[nodiscard]] inline std::uint8_t GetTaskQMax() const noexcept {
            return TaskQMax;
        }
        [[nodiscard]] inline std::uint8_t GetTaskQTick() const noexcept {
            return TaskQTick;
        }
        [[nodiscard]] inline bool GetDirectTaskQ() const noexcept {
            return DirectTaskQ;
        }
        [[nodiscard]] inline bool GetDivideTaskQ() const noexcept {
            return DivideTaskQ;
        }
        [[nodiscard]] inline std::uint8_t GetUpdateDelayTick() const noexcept {
            return UpdateDelayTick;
        }
        [[nodiscard]] inline std::uint32_t GetHotKey1() const noexcept {
            return HotKey1;
        }
        [[nodiscard]] inline std::uint32_t GetHotKey2() const noexcept {
            return HotKey2;
        }
        [[nodiscard]] inline float GetWeldDistance() const noexcept {
            return WeldDistance;
        }
        [[nodiscard]] inline float GetNormalSmoothDegree() const noexcept {
            return NormalSmoothDegree;
        }
        [[nodiscard]] inline std::uint8_t GetSubdivision() const noexcept {
            return Subdivision;
        }
        [[nodiscard]] inline std::uint8_t GetVertexSmooth() const noexcept {
            return VertexSmooth;
        }
        [[nodiscard]] inline float GetVertexSmoothStrength() const noexcept {
            return VertexSmoothStrength;
        }
        [[nodiscard]] inline std::uint32_t GetTextureMargin() const noexcept {
            return TextureMargin;
        }
        [[nodiscard]] inline bool GetTextureMarginGPU() const noexcept {
            return TextureMarginGPU;
        }
        [[nodiscard]] inline bool GetTangentZCorrection() const noexcept {
            return TangentZCorrection;
        }

        inline void SetTaskQMax(std::uint8_t newTaskQMax) {
            TaskQMax = newTaskQMax;
            logger::info("Set TaskQMax {}", newTaskQMax);
        }
        inline void SetTaskQTick(std::uint8_t newTaskQTick) {
            TaskQTick = newTaskQTick;
            logger::info("Set TaskQTick {}", newTaskQTick);
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

        //General
        bool PlayerEnable = true;
        bool NPCEnable = true;
        bool MaleEnable = true;
        bool FemaleEnable = true;
        bool HeadEnable = false;
        bool RealtimeDetect = true;
        std::uint8_t RealtimeDetectHead = 1; //0 disable, 1 morph data only, all data

        bool GPUEnable = true;
        unsigned long PriorityCores = 0;
        unsigned long PriorityCoreCount = 0;

        std::uint8_t AutoTaskQ = AutoTaskQList::Disable; //0 disable, 1 speed focused, 2 speed focused but performance focused, 3 balanced, 4 performance focused
        std::uint8_t TaskQMax = 1;
        std::clock_t TaskQTick = 13;
        bool DirectTaskQ = false;
        std::uint8_t DivideTaskQ = 1;

        std::uint8_t UpdateDelayTick = 1;

        std::uint32_t HotKey1 = 0;
        std::uint32_t HotKey2 = 43;

        float WeldDistance = 0.0001f;
        float NormalSmoothDegree = 60.0f;
        std::uint8_t Subdivision = 0;
        std::uint8_t VertexSmooth = 0;
        float VertexSmoothStrength = 0.5f;

        std::uint32_t TextureMargin = 2;
        bool TextureMarginGPU = true;
        bool TangentZCorrection = true;

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
}
