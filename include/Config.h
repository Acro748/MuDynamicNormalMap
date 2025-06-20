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

        //Debug
        [[nodiscard]] inline spdlog::level::level_enum GetLogLevel() const noexcept {
            return logLevel;
        }
        [[nodiscard]] inline spdlog::level::level_enum GetFlushLevel() const noexcept {
            return flushLevel;
        }

        //General
		[[nodiscard]] inline std::uint32_t GetDefaultTextureHeight() const noexcept {
			return DefaultTextureHeight;
		}
		[[nodiscard]] inline std::uint32_t GetDefaultTextureWidth() const noexcept {
			return DefaultTextureWidth;
		}
		[[nodiscard]] inline float GetTextureResize() const noexcept {
			return TextureResize;
		}
        [[nodiscard]] inline bool GetIgnoreTextureSize() const noexcept {
            return IgnoreTextureSize;
        }

        //NormalmapBake
        [[nodiscard]] inline bool GetBakeEnable() const noexcept {
            return BakeEnable;
        }
        [[nodiscard]] inline bool GetPlayerEnable() const noexcept {
            return PlayerEnable;
        }
        [[nodiscard]] inline bool GetNPCEnable() const noexcept {
            return NPCEnable;
        }
        [[nodiscard]] inline bool GetHeadEnable() const noexcept {
            return HeadEnable;
        }
        [[nodiscard]] inline unsigned long GetPriorityCores() const noexcept {
            return PriorityCores;
        }
        [[nodiscard]] inline std::uint8_t GetNormalmapBakeDelayTick() const noexcept {
            return NormalmapBakeDelayTick;
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

    protected:
        //Debug
        spdlog::level::level_enum logLevel{ spdlog::level::level_enum::info };
        spdlog::level::level_enum flushLevel{ spdlog::level::level_enum::trace };

        //General
        std::uint32_t DefaultTextureHeight = 2048.0f;
        std::uint32_t DefaultTextureWidth = 2048.0f;
		float TextureResize = 1.0f;
		bool IgnoreTextureSize = false;

        //NormalmapBake
        bool BakeEnable = true;
        bool PlayerEnable = true;
        bool NPCEnable = true;
        bool HeadEnable = false;
        unsigned long PriorityCores = -1;
        std::uint8_t NormalmapBakeDelayTick = 2;
        float NormalSmoothDegree = 60.0f;
        std::uint8_t Subdivision = 1;
        std::uint8_t VertexSmooth = 1;
        float VertexSmoothStrength = 0.5f;

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
        bool LoadBakeNormalMapMaskTexture();

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
