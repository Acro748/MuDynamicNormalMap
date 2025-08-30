#include "Config.h"

using namespace Mus;

namespace Mus {
    bool Config::LoadLogging()
    {
        std::string configPath = GetRuntimeSKSEDirectory();
        configPath += SKSE::PluginDeclaration::GetSingleton()->GetName().data();
        configPath += ".ini";

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
        configPath += SKSE::PluginDeclaration::GetSingleton()->GetName().data();
        configPath += ".ini";

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
                else if (variableName == "QueueTime")
                {
                    QueueTime = GetBoolValue(variableValue);
                }
                else if (variableName == "FullUpdateTime")
                {
                    FullUpdateTime = GetBoolValue(variableValue);
                }
                else if (variableName == "ActorVertexHasherTime1")
                {
                    ActorVertexHasherTime1 = GetBoolValue(variableValue);
                }
                else if (variableName == "ActorVertexHasherTime2")
                {
                    ActorVertexHasherTime2 = GetBoolValue(variableValue);
                }
                else if (variableName == "GeometryDataTime")
                {
                    GeometryDataTime = GetBoolValue(variableValue);
                }
                else if (variableName == "UpdateNormalMapTime1")
                {
                    UpdateNormalMapTime1 = GetBoolValue(variableValue);
                }
                else if (variableName == "UpdateNormalMapTime2")
                {
                    UpdateNormalMapTime2 = GetBoolValue(variableValue);
                }
                else if (variableName == "BleedTextureTime1")
                {
                    BleedTextureTime1 = GetBoolValue(variableValue);
                }
                else if (variableName == "BleedTextureTime2")
                {
                    BleedTextureTime2 = GetBoolValue(variableValue);
                }
                else if (variableName == "TextureCopyTime")
                {
                    TextureCopyTime = GetBoolValue(variableValue);
                }
                else if (variableName == "MergeTime1")
                {
                    MergeTime1 = GetBoolValue(variableValue);
                }
                else if (variableName == "MergeTime2")
                {
                    MergeTime2 = GetBoolValue(variableValue);
                }
                else if (variableName == "CompressTime")
                {
                    CompressTime = GetBoolValue(variableValue);
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
                else if (variableName == "HotKey1")
                {
                    HotKey1 = GetUIntValue(variableValue);
                }
                else if (variableName == "HotKey2")
                {
                    HotKey2 = GetUIntValue(variableValue);
                }
                else if (variableName == "RevertNormalMap")
                {
                    RevertNormalMap = GetBoolValue(variableValue);
                }
                else if (variableName == "UpdateDelayTick")
                {
                    UpdateDelayTick = GetUIntValue(variableValue);
                }
                else if (variableName == "ApplyOverlay")
                {
                    ApplyOverlay = GetBoolValue(variableValue);
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
            else if (currentSetting == "[Geometry]")
            {
                if (variableName == "WeldDistance")
                {
                    WeldDistance = std::max(GetFloatValue(variableValue), 0.0001f);
                }
                else if (variableName == "BoundaryWeldDistance")
                {
                    BoundaryWeldDistance = std::max(GetFloatValue(variableValue), 0.0001f);
                }
                else if (variableName == "NormalSmoothDegree")
                {
                    NormalSmoothDegree = std::clamp(GetFloatValue(variableValue), 0.0f, 180.0f);
                }
                else if (variableName == "AllowInvertNormalSmooth")
                {
                    AllowInvertNormalSmooth = GetBoolValue(variableValue);
                }
                else if (variableName == "Subdivision")
                {
                    Subdivision = GetUIntValue(variableValue);
                }
                else if (variableName == "SubdivisionTriThreshold")
                {
                    SubdivisionTriThreshold = GetUIntValue(variableValue);
                }
                else if (variableName == "VertexSmooth")
                {
                    VertexSmooth = GetUIntValue(variableValue);
                }
                else if (variableName == "VertexSmoothStrength")
                {
                    VertexSmoothStrength = std::clamp(GetFloatValue(variableValue), 0.0f, 1.0f);
                }
                else if (variableName == "VertexSmoothByAngle")
                {
                    VertexSmoothByAngle = GetUIntValue(variableValue);
                }
                else if (variableName == "VertexSmoothByAngleThreshold1")
                {
                    VertexSmoothByAngleThreshold1 = std::clamp(GetFloatValue(variableValue), 0.0f, 180.0f);
                }
                else if (variableName == "VertexSmoothByAngleThreshold2")
                {
                    VertexSmoothByAngleThreshold2 = std::clamp(GetFloatValue(variableValue), 0.0f, 180.0f);
                }
            }
            else if (currentSetting == "[Texture]")
            {
                if (variableName == "TextureWidth")
				{
                    TextureWidth = GetUIntValue(variableValue);
				}
				else if (variableName == "TextureHeight")
				{
                    TextureHeight = GetUIntValue(variableValue);
				}
				else if (variableName == "TextureMargin")
				{
                    TextureMargin = GetIntValue(variableValue);
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
            else if (currentSetting == "[Performance]")
            {
                if (variableName == "GPUEnable")
                {
                    GPUEnable = GetBoolValue(variableValue);
                }
                else if (variableName == "TextureMarginGPU")
                {
                    TextureMarginGPU = GetBoolValue(variableValue);
                }
                else if (variableName == "MergeTextureGPU")
                {
                    MergeTextureGPU = GetBoolValue(variableValue);
                }
                else if (variableName == "WaitForRendererTickMS")
                {
                    WaitForRendererTickMS = GetUIntValue(variableValue);
                }
                else if (variableName == "UpdateDistance")
                {
                    float value = GetFloatValue(variableValue);
                    UpdateDistance = value * value;
                }
                else if (variableName == "UpdateDistanceVramSave")
                {
                    UpdateDistanceVramSave = GetBoolValue(variableValue);
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
                else if (variableName == "VRAMSaveMode")
                {
                    VRAMSaveMode = GetBoolValue(variableValue);
                }
                else if (variableName == "TextureCompress")
                {
                    TextureCompress = GetUIntValue(variableValue);
                }
            }
            else if (currentSetting == "[RealtimeDetect]")
            {
                if (variableName == "RealtimeDetect")
                {
                    RealtimeDetect = GetBoolValue(variableValue);
                }
                else if (variableName == "RealtimeDetectHead")
                {
                    RealtimeDetectHead = GetUIntValue(variableValue);
                }
                else if (variableName == "RealtimeDetectOnBackGround")
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
            }
        }
        if (UpdateDistance > floatPrecision && DetectDistance > floatPrecision)
            UpdateDistance = std::max(DetectDistance, UpdateDistance);
        else
            UpdateDistance = 0.0f;
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
                    else if (variableName == "ProxyFirstScan")
                    {
                        condition.ProxyFirstScan = GetBoolValue(variableValue);
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

    void InitialSetting()
    {
        std::uint32_t actorThreads = 2;
        std::uint32_t memoryManageThreads = 3;
        std::uint32_t processingThreads = std::thread::hardware_concurrency();
        if (Mus::Config::GetSingleton().GetAutoTaskQ() > 0)
        {
            //float benchMarkResult = Mus::miniBenchMark();
            //55~high-end, 35~middle-end, 15~low-end
            //logger::info("CPU bench mark score : {}", (std::uint32_t)benchMarkResult);
            //float CPUPerformanceMult = std::max(1.0f, 55.0f / benchMarkResult);
            float gpuBenchMarkResult = Mus::miniBenchMarkGPU();
            float GPUPerformanceMult = 1.0f;
            if (gpuBenchMarkResult < 0.0f) {
            }
            else
            {
                //500 middle-end
                logger::info("GPU bench mark score : {}", (std::int32_t)gpuBenchMarkResult);
                GPUPerformanceMult = std::max(1.0f, 500.0f / gpuBenchMarkResult);
            }

            switch (Mus::Config::GetSingleton().GetAutoTaskQ()) {
            case Mus::Config::AutoTaskQList::Fastest:
                Mus::Config::GetSingleton().SetTaskQMax(2);
                Mus::Config::GetSingleton().SetTaskQTickMS(0);
                Mus::Config::GetSingleton().SetDirectTaskQ(true);
                Mus::Config::GetSingleton().SetDivideTaskQ(0);
                Mus::Config::GetSingleton().SetVRAMSaveMode(false);
                actorThreads = 2;
                memoryManageThreads = 2;
                break;
            case Mus::Config::AutoTaskQList::Faster:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTickMS(0);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(0);
                Mus::Config::GetSingleton().SetVRAMSaveMode(false);
                actorThreads = 1;
                memoryManageThreads = 1;
                break;
            case Mus::Config::AutoTaskQList::Balanced:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTickMS(Mus::TaskQTickBase * GPUPerformanceMult);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(0);
                Mus::Config::GetSingleton().SetVRAMSaveMode(true);
                actorThreads = 1;
                memoryManageThreads = 1;
                break;
            case Mus::Config::AutoTaskQList::BetterPerformance:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTickMS(Mus::TaskQTickBase * GPUPerformanceMult);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(2);
                Mus::Config::GetSingleton().SetVRAMSaveMode(true);
                actorThreads = 1;
                memoryManageThreads = 1;
                break;
            case Mus::Config::AutoTaskQList::BestPerformance:
                Mus::Config::GetSingleton().SetTaskQMax(1);
                Mus::Config::GetSingleton().SetTaskQTickMS(Mus::TaskQTickBase * GPUPerformanceMult * 2);
                Mus::Config::GetSingleton().SetDirectTaskQ(false);
                Mus::Config::GetSingleton().SetDivideTaskQ(2);
                Mus::Config::GetSingleton().SetVRAMSaveMode(true);
                actorThreads = 1;
                memoryManageThreads = 1;
                break;
            default:
                break;
            }
        }

        Mus::gpuTask = std::make_unique<Mus::ThreadPool_GPUTaskModule>(0, Mus::Config::GetSingleton().GetDirectTaskQ(), Mus::Config::GetSingleton().GetTaskQMax());
        Mus::g_frameEventDispatcher.addListener(Mus::gpuTask.get());

        Mus::actorThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(actorThreads);
        logger::info("set actorThreads {}", actorThreads);

        Mus::memoryManageThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(memoryManageThreads);
        logger::info("set memoryManageThreads {}", memoryManageThreads);

        Mus::processingThreads = std::make_unique<Mus::ThreadPool_ParallelModule>(processingThreads);
        logger::info("set processingThreads {}", processingThreads);

        Mus::weldDistance = std::max(0.0001f, Mus::Config::GetSingleton().GetWeldDistance());
        Mus::weldDistanceMult = 1.0f / Mus::weldDistance;

        Mus::boundaryWeldDistance = std::max(0.0001f, Mus::Config::GetSingleton().GetBoundaryWeldDistance());
        Mus::boundaryWeldDistanceMult = 1.0f / Mus::boundaryWeldDistance;

        if (Mus::Config::GetSingleton().GetRealtimeDetectOnBackGround())
        {
            Mus::g_armorAttachEventEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
            Mus::g_facegenNiNodeEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
            Mus::g_actorChangeHeadPartEventDispatcher.addListener(&Mus::ActorVertexHasher::GetSingleton());
            Mus::ActorVertexHasher::GetSingleton().Init();
        }
    }
}

