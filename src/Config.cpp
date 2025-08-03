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
                else if (variableName == "flushLevel")
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
                if (variableName == "logLevel")
                {
                    logLevel = spdlog::level::from_str(variableValue);
                }
                else if (variableName == "flushLevel")
                {
                    flushLevel = spdlog::level::from_str(variableValue);
                }
                else if (variableName == "DebugTexture")
                {
                    DebugTexture = GetBoolValue(variableValue);
                }
                else if (variableName == "PerformanceLog")
                {
                    PerformanceLog = GetBoolValue(variableValue);
                    PerformanceCheck = PerformanceLog;
                }
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
                else if (variableName == "DetectTickMS")
				{
                    DetectTickMS = GetIntValue(variableValue);
				}
                else if (variableName == "RemoveBeforeNormalMap")
                {
                    RemoveBeforeNormalMap = GetBoolValue(variableValue);
                }
                else if (variableName == "UpdateDistance")
                {
                    float value = GetFloatValue(variableValue);
                    UpdateDistance = value * value;
                }
                else if (variableName == "GPUEnable")
                {
					GPUEnable = GetBoolValue(variableValue);
                }
                else if (variableName == "WaitForRendererTickMS")
                {
                    WaitForRendererTickMS = GetUIntValue(variableValue);
                }
                else if (variableName == "PriorityCores")
				{
                    auto list = split(variableValue, ',');
                    for (auto& c : list) {
                        PriorityCoreList.insert(GetUIntValue(c));
                    }
				}
                else if (variableName == "DetectPriorityCores")
                {
                    DetectPriorityCores = GetIntValue(variableValue);
                }
				else if (variableName == "AutoTaskQ")
				{
                    AutoTaskQ = std::min(std::uint32_t(AutoTaskQList::Total - 1), GetUIntValue(variableValue));
				}
				else if (variableName == "TaskQMax")
				{
                    TaskQMax = GetUIntValue(variableValue);
				}
				else if (variableName == "TaskQTickMS")
				{
                    TaskQTickMS = GetUIntValue(variableValue);
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
                    TextureMargin = GetIntValue(variableValue);
				}
				else if (variableName == "TextureMarginGPU")
				{
                    TextureMarginGPU = GetBoolValue(variableValue);
				}
				else if (variableName == "MergeTextureGPU")
				{
                    MergeTextureGPU = GetBoolValue(variableValue);
				}
				else if (variableName == "TextureCompress")
				{
                    TextureCompress = GetIntValue(variableValue);
				}
				else if (variableName == "TextureWidth")
				{
                    TextureWidth = GetUIntValue(variableValue);
				}
				else if (variableName == "TextureHeight")
				{
                    TextureHeight = GetUIntValue(variableValue);
				}
                else if (variableName == "BlueRadius")
                {
                    BlueRadius = GetUIntValue(variableValue);
                }
				else if (variableName == "TangentZCorrection")
				{
                    TangentZCorrection = GetBoolValue(variableValue);
				}
				else if (variableName == "DetailStrength")
				{
                    DetailStrength = std::clamp(GetFloatValue(variableValue), 0.0f, 1.0f);
				}
                else if (variableName == "RemoveSkinOverrides")
                {
                    RemoveSkinOverrides = GetBoolValue(variableValue);
                }
                else if (variableName == "RemoveNodeOverrides")
                {
                    RemoveNodeOverrides = GetBoolValue(variableValue);
                }
			}
        }
        UpdateDistance = std::max(DetectDistance, UpdateDistance);
        return true;
    }

    void Config::SetPriorityCores(std::int32_t detectPriorityCores)
    {
        if (detectPriorityCores == -1)
            detectPriorityCores = DetectPriorityCores;

        PriorityCoreMask = 0;
        std::uint32_t cores = std::thread::hardware_concurrency();
        logger::info("Detected cores : {}", cores);
        if (PriorityCoreList.empty())
        {
            PriorityCoreCount = std::max(2.0, cores / (std::pow(2, detectPriorityCores)));
            logger::info("Enable cores for task : {}", PriorityCoreCount);
        }
        else
        {
            auto PriorityCoreList_ = PriorityCoreList;
            auto PriorityCoreList__ = PriorityCoreList_;
            PriorityCoreList_.clear();
            for (auto& core : PriorityCoreList__) {
                if (core < 0 || core >= cores)
                    continue;
                PriorityCoreList_.insert(core);
            }
            if (detectPriorityCores > 0)
            {
                detectPriorityCores = cores / (std::pow(2, detectPriorityCores));
                detectPriorityCores = std::max(std::int32_t(1), detectPriorityCores);
                detectPriorityCores -= PriorityCoreList_.size();
                if (detectPriorityCores > 0)
                {
                    for (std::int32_t i = cores; i > 1; i--) { //excluding first core
                        std::int32_t coreNum = i - 1;
                        if (PriorityCoreList_.find(coreNum) != PriorityCoreList_.end())
                            continue;
                        PriorityCoreList_.insert(coreNum);
                        detectPriorityCores--;
                        if (detectPriorityCores == 0)
                            break;
                    }
                }
            }
            std::string coreList = "";
            for (auto core : PriorityCoreList_) {
                if (!coreList.empty())
                    coreList += ", ";
                coreList += std::to_string(core);
                PriorityCoreMask |= 1 << core;
                PriorityCoreCount += 1;
            }
            logger::info("Enable cores for task : {} / {:x}", coreList, PriorityCoreMask);
        }
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
            bool isConditionState = false;

            std::string line;
            while (std::getline(ifile, line))
            {
                skipComments(line);
                trim(line);
                if (line.length() > 0)
                {
                    std::string variableName;
                    std::string variableValue = GetConfigSetting(line, variableName);
                    isConditionState = variableName == "Condition";

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
                    else if (variableName == "DynamicTriShapeAsHead")
                    {
                        condition.DynamicTriShapeAsHead = GetBoolValue(variableValue);
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "DetailStrength")
                    {
                        condition.DetailStrength = GetFloatValue(variableValue);
                        isNormalConditionFile = true;
                    }
                    else if (variableName == "ProxyDetailTextureFolder" || variableName == "ProxyTangentTextureFolder")
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
                    else if (variableName == "Condition")
                    {
                        condition.originalCondition = variableValue;
                        isNormalConditionFile = true;
                    }
                    else
                    {
                        if (isConditionState)
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

