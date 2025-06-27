#include "Config.h"

using namespace Mus;

namespace Mus {
    bool Config::LoadLogging()
    {
        std::string configPath = GetRuntimeSKSEDirectory();
        configPath += "MuDynamicNormalMap.ini";

        std::ifstream file(configPath);

        if (!file.is_open())
        {
            std::transform(configPath.begin(), configPath.end(), configPath.begin(), ::tolower);
            file.open(configPath);
        }

        if (!file.is_open())
        {
            return false;
        }

        std::string line;
        std::string currentSetting;
        while (std::getline(file, line))
        {
            //trim(line);
            skipComments(line);
            trim(line);
            if (line.length() == 0)
                continue;

            if (line.substr(0, 1) == "[")
            {
                currentSetting = line;
                continue;
            }
            std::string variableName;
            std::string variableValue = GetConfigSetting(line, variableName);
            if (currentSetting == "[Debug]")
            {
                if (variableName == "logLevel")
                {
                    logLevel = spdlog::level::from_str(variableValue);
                }
                if (variableName == "flushLevel")
                {
                    flushLevel = spdlog::level::from_str(variableValue);
                }
            }
        }
        return true;
    }

    bool Config::LoadConfig() {
        std::string configPath = GetRuntimeSKSEDirectory();
        configPath += "MuDynamicNormalMap.ini";

        std::ifstream file(configPath);

        if (!file.is_open())
        {
			lowLetter(configPath);

            file.open(configPath);
            if (!file.is_open())
            {
                logger::critical("Unable to load Config file.");
                return false;
            }
        }

        return LoadConfig(file);
    }

    bool Config::LoadConfig(std::ifstream& configfile)
    {
        std::int32_t detectPriorityCores = 0;
        std::unordered_set<std::int32_t> priorityCores;

        std::string line;
        std::string currentSetting;
        while (std::getline(configfile, line))
        {
            //trim(line);
            skipComments(line);
            trim(line);
            if (line.length() == 0)
                continue;

            if (line.substr(0, 1) == "[")
            {
                currentSetting = line;
                continue;
            }
            std::string variableName;
            std::string variableValue = GetConfigSetting(line, variableName);
            if (currentSetting == "[Debug]")
            {

            }
            else if (currentSetting == "[General]")
            {
                if (variableName == "DefaultTextureWidth")
                {
                    DefaultTextureWidth = GetUIntValue(variableValue);
                }
                else if (variableName == "DefaultTextureHeight")
                {
                    DefaultTextureHeight = GetUIntValue(variableValue);
                }
				else if (variableName == "TextureResize")
				{
					TextureResize = GetFloatValue(variableValue);
				}
				else if (variableName == "IgnoreTextureSize")
				{
					IgnoreTextureSize = GetBoolValue(variableValue);
				}
			}
            else if (currentSetting == "[NormalmapBake]")
            {
                if (variableName == "BakeEnable")
				{
                    BakeEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "PlayerEnable")
				{
                    PlayerEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "NPCEnable")
				{
                    NPCEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "HeadEnable")
				{
                    HeadEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "PriorityCores")
				{
                    auto list = split(variableValue, ',');
                    for (auto& c : list) {
                        priorityCores.insert(GetUIntValue(c));
                    }
				}
                else if (variableName == "DetectPriorityCores")
                {
                    detectPriorityCores = GetIntValue(variableValue);
                }
				else if (variableName == "NormalmapBakeDelayTick")
				{
                    NormalmapBakeDelayTick = GetUIntValue(variableValue);
				}
				else if (variableName == "BakeKey1")
				{
                    BakeKey1 = GetUIntValue(variableValue);
				}
				else if (variableName == "BakeKey2")
				{
                    BakeKey2 = GetUIntValue(variableValue);
				}
				else if (variableName == "WeldDistance")
				{
                    WeldDistance = GetFloatValue(variableValue);
				}
				else if (variableName == "TextureMargin")
				{
                    TextureMargin = GetIntValue(variableValue);
				}
				else if (variableName == "NormalSmoothDegree")
				{
                    NormalSmoothDegree = GetFloatValue(variableValue);
                    NormalSmoothDegree = std::clamp(NormalSmoothDegree, 0.0f, 90.0f);
				}
				else if (variableName == "Subdivision")
				{
                    Subdivision = GetUIntValue(variableValue);
				}
				else if (variableName == "VertexSmooth")
				{
                    VertexSmooth = GetUIntValue(variableValue);
				}
				else if (variableName == "VertexSmoothStrength")
				{
                    VertexSmoothStrength = GetFloatValue(variableValue);
				}
			}
        }

        PriorityCores = 0;
        std::uint32_t cores = std::thread::hardware_concurrency();
        logger::info("Detected cores : {}", cores);
        auto priorityCores_ = priorityCores;
        priorityCores.clear();
        for (auto& core : priorityCores_) {
            if (core < 0 || core >= cores)
                continue;
            priorityCores.insert(core);
        }
        if (priorityCores.empty() && detectPriorityCores == 0)
            detectPriorityCores = 1;
        if (detectPriorityCores > 0)
        {
            detectPriorityCores = cores / (std::pow(2, detectPriorityCores));
            detectPriorityCores = std::max(std::int32_t(1), detectPriorityCores);
            detectPriorityCores -= priorityCores.size();
            if (detectPriorityCores > 0)
            {
                for (std::int32_t i = cores; i > 1; i--) { //excluding first core
                    std::int32_t coreNum = i - 1;
                    if (priorityCores.find(coreNum) != priorityCores.end())
                        continue;
                    priorityCores.insert(coreNum);
                    detectPriorityCores--;
                    if (detectPriorityCores == 0)
                        break;
                }
            }
        }
        std::string coreList = "";
        for (auto core : priorityCores) {
            if (!coreList.empty())
                coreList += ", ";
            coreList += std::to_string(core);
            PriorityCores |= 1 << core;
            PriorityCoreCount += 1;
        }
        logger::info("Enable cores for baking normalmap : {} / {:x}", coreList, PriorityCores);

        return true;
    }

    bool MultipleConfig::LoadBakeNormalMapMaskTexture()
    {
        std::string configPath = GetRuntimeSKSEDirectory();
        configPath += "MuDynamicNormalMap\\BakeObjectNormalMap";

        for (auto& file : GetAllFiles(configPath))
        {

        }
        return false;
    }
}

