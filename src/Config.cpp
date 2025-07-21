#include "Config.h"

using namespace Mus;

namespace Mus {
    bool Config::LoadLogging()
    {
        std::string configPath = GetRuntimeSKSEDirectory();
        configPath += SKSE::PluginDeclaration::GetSingleton()->GetName().data();
        configPath += +".ini";

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
                if (variableName == "PlayerEnable")
				{
                    PlayerEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "NPCEnable")
				{
                    NPCEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "MaleEnable")
				{
                    MaleEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "FemaleEnable")
				{
                    FemaleEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "HeadEnable")
				{
                    HeadEnable = GetBoolValue(variableValue);
				}
                else if (variableName == "RealtimeDetect")
				{
                    RealtimeDetect = GetBoolValue(variableValue);
				}
                else if (variableName == "RealtimeDetectHead")
				{
                    RealtimeDetectHead = GetUIntValue(variableValue);
				}
                else if (variableName == "RealtimeDetectHead")
				{
                    RealtimeDetectOnBackGround = GetBoolValue(variableValue);
				}
                else if (variableName == "DetectDistance")
				{
                    float value = GetFloatValue(variableValue);
                    DetectDistance = value * value;
				}
                else if (variableName == "DetectTick")
				{
                    DetectTick = GetIntValue(variableValue);
				}
                else if (variableName == "GPUEnable")
                {
					GPUEnable = GetBoolValue(variableValue);
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
				else if (variableName == "AutoTaskQ")
				{
                    AutoTaskQ = std::min(std::uint32_t(AutoTaskQList::Total - 1), GetUIntValue(variableValue));
				}
				else if (variableName == "TaskQMax")
				{
                    TaskQMax = GetUIntValue(variableValue);
				}
				else if (variableName == "TaskQTick")
				{
                    TaskQTick = GetUIntValue(variableValue);
				}
				else if (variableName == "DirectTaskQ")
				{
                    DirectTaskQ = GetBoolValue(variableValue);
				}
				else if (variableName == "DivideTaskQ")
				{
                    DivideTaskQ = GetUIntValue(variableValue);
				}
				else if (variableName == "UpdateDelayTick")
				{
                    UpdateDelayTick = GetUIntValue(variableValue);
				}
				else if (variableName == "HotKey1")
				{
                    HotKey1 = GetUIntValue(variableValue);
				}
				else if (variableName == "HotKey2")
				{
                    HotKey2 = GetUIntValue(variableValue);
				}
				else if (variableName == "WeldDistance")
				{
                    WeldDistance = GetFloatValue(variableValue);
				}
				else if (variableName == "NormalSmoothDegree")
				{
                    NormalSmoothDegree = std::clamp(GetFloatValue(variableValue), 0.0f, 180.0f);
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
				else if (variableName == "TextureMargin")
				{
                    TextureMargin = GetUIntValue(variableValue);
				}
				else if (variableName == "TextureMarginGPU")
				{
                    TextureMarginGPU = GetBoolValue(variableValue);
				}
				else if (variableName == "TangentZCorrection")
				{
                    TangentZCorrection = GetBoolValue(variableValue);
				}
				else if (variableName == "DetailStrength")
				{
                    DetailStrength = std::clamp(GetFloatValue(variableValue), 0.0f, 1.0f);
				}
			}
        }

        PriorityCores = 0;
        std::uint32_t cores = std::thread::hardware_concurrency();
        logger::info("Detected cores : {}", cores);
        if (priorityCores.empty())
        {
            PriorityCoreCount = std::max(2.0, cores / (std::pow(2, detectPriorityCores)));
            logger::info("Enable cores for task : {}", PriorityCoreCount);
        }
        else
        {
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
            logger::info("Enable cores for task : {} / {:x}", coreList, PriorityCores);
        }

        return true;
    }

    bool MultipleConfig::LoadConditionFile()
    {
        std::string conditionPath = GetRuntimeSKSEDirectory();
        conditionPath += SKSE::PluginDeclaration::GetSingleton()->GetName().data();
        auto files = GetAllFiles(conditionPath);
        concurrency::parallel_for_each(files.begin(), files.end(), [&](auto& file) {
            std::u8string filename_utf8 = file.filename().u8string();
            std::string filename(filename_utf8.begin(), filename_utf8.end());
            if (filename == "." || filename == "..")
                return;
            if (!stringEndsWith(filename, ".ini"))
                return;
            std::ifstream ifile(file);
            if (!ifile.is_open())
                return;

            logger::info("File found: {}", filename);

            ConditionManager::Condition condition;
            condition.fileName = filename;
			condition.HeadEnable = GetHeadEnable();
			condition.DetailStrength = GetDetailStrength();

            bool isNormalConditionFile = false;

            std::string line;
            while (std::getline(ifile, line))
            {
                skipComments(line);
                trim(line);
                if (line.length() > 0)
                {
                    std::string variableName;
                    std::string variableValue = GetConfigSetting(line, variableName);
                    if (variableName == "Enable")
                    {
                        condition.Enable = GetBoolValue(variableValue);
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "HeadEnable")
                    {
                        condition.HeadEnable = GetBoolValue(variableValue);
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "DetailStrength")
                    {
                        condition.DetailStrength = GetFloatValue(variableValue);
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "ProxyDetailTextureFolder")
                    {
                        if (!variableValue.empty())
                        {
                            for (auto& value : split(variableValue, ','))
                            {
                                condition.ProxyDetailTextureFolder.push_back(value);
                            }
                        }
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "ProxyOverlayTextureFolder")
                    {
                        if (!variableValue.empty())
                        {
                            for (auto& value : split(variableValue, ','))
                            {
                                condition.ProxyOverlayTextureFolder.push_back(value);
                            }
                        }
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "ProxyMaskTextureFolder")
                    {
                        if (!variableValue.empty())
                        {
                            for (auto& value : split(variableValue, ','))
                            {
                                condition.ProxyMaskTextureFolder.push_back(value);
                            }
                        }
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "Priority")
                    {
                        condition.Priority = GetIntValue(variableValue);
                        isNormalConditionFile = true;
                    }
                    else if (variableValue == "Condition")
                    {
                        condition.originalCondition = variableValue;
                        isNormalConditionFile = true;
                    }
                    else
                    {
                        condition.originalCondition += " " + variableValue;
                    }
                }
            }
            if (isNormalConditionFile)
                ConditionManager::GetSingleton().RegisterCondition(condition);
        });
        return false;
    }
}

