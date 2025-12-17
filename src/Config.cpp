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
                else if (variableName == "MergeTime")
                {
                    MergeTime = GetBoolValue(variableValue);
                }
                else if (variableName == "GenerateMipsTime")
                {
                    GenerateMipsTime = GetBoolValue(variableValue);
                }
                else if (variableName == "BleedTextureTime1")
                {
                    BleedTextureTime1 = GetBoolValue(variableValue);
                }
                else if (variableName == "BleedTextureTime2")
                {
                    BleedTextureTime2 = GetBoolValue(variableValue);
                }
                else if (variableName == "CompressTime")
                {
                    CompressTime = GetBoolValue(variableValue);
                }
                else if (variableName == "TextureCopyTime")
                {
                    TextureCopyTime = GetBoolValue(variableValue);
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
                else if (variableName == "UseConsoleRef")
                {
                    UseConsoleRef = GetBoolValue(variableValue);
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
                    WeldDistance = std::max(GetFloatValue(variableValue), floatPrecision);
                }
                else if (variableName == "BoundaryWeldDistance")
                {
                    BoundaryWeldDistance = std::max(GetFloatValue(variableValue), floatPrecision);
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
				else if (variableName == "TangentZCorrection")
				{
                    TangentZCorrection = GetBoolValue(variableValue);
				}
				else if (variableName == "DetailStrength")
				{
                    DetailStrength = std::clamp(GetFloatValue(variableValue), 0.0f, 1.0f);
				}
				else if (variableName == "IgnoreMissingNormalMap")
				{
                    IgnoreMissingNormalMap = GetBoolValue(variableValue);
				}
            }
            else if (currentSetting == "[Performance]")
            {
                if (variableName == "GPUEnable")
                {
                    GPUEnable = GetBoolValue(variableValue);
                }
                else if (variableName == "GPUDeviceIndex")
                {
                    GPUDeviceIndex = GetIntValue(variableValue);
                }
                else if (variableName == "GPUForceSync")
                {
                    GPUForceSync = GetBoolValue(variableValue);
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
                else if (variableName == "UseMipMap")
                {
                    UseMipMap = GetBoolValue(variableValue);
                }
                else if (variableName == "AutoTaskQ")
                {
                    AutoTaskQ = std::min(std::uint32_t(AutoTaskQList::Total - 1), GetUIntValue(variableValue));
                }
                else if (variableName == "TaskQTickMS")
                {
                    TaskQTickMS = GetUIntValue(variableValue);
                }
                else if (variableName == "TextureCompress")
                {
                    TextureCompress = GetIntValue(variableValue);
                }
                else if (variableName == "TextureCompressQuality")
                {
                    TextureCompressQuality = GetUIntValue(variableValue);
                }
                else if (variableName == "DiskCache")
                {
                    DiskCache = GetBoolValue(variableValue);
                }
                else if (variableName == "DiskCacheFolder")
                {
                    DiskCacheFolder = FixPath(variableValue);
                }
                else if (variableName == "DiskCacheLimitMB")
                {
                    DiskCacheLimitMB = GetUIntValue(variableValue);
                }
                else if (variableName == "ClearDiskCache")
                {
                    ClearDiskCache = GetBoolValue(variableValue);
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
        Shader::ShaderManager::GetSingleton().SetSearchSecondGPU(Config::GetSingleton().GetGPUDeviceIndex() != 0);
        ObjectNormalMapUpdater::GetSingleton().ClearGeometryResourceData();

        bool isSecondGPUEnabled = Config::GetSingleton().GetGPUDeviceIndex() != 0 && Shader::ShaderManager::GetSingleton().IsValidSecondGPU();

		const std::int32_t coreCount = std::thread::hardware_concurrency();
        std::int32_t actorThreadCount = 1;
        std::int32_t memoryManageThreadCount = 1;
        std::int32_t updateThreadCount = 1;
        std::int32_t processingThreadCount = coreCount / 2;
		std::int32_t gpuTaskThreadCount = 1;
        isNoSplitGPU = false;
		bool waitPreTask = false;
        std::clock_t taskQTickMS = 1000.0f;
        bool directTaskQ = false;
        bool usePCores = false;
        if (Config::GetSingleton().GetAutoTaskQ() > 0)
        {
            switch (Config::GetSingleton().GetAutoTaskQ()) {
            default:
            case Config::AutoTaskQList::Fastest:
                taskQTickMS = 0;
                directTaskQ = true;
                divideTaskQ = 0;
                vramSaveMode = false;

                isNoSplitGPU = true;

                waitPreTask = false;
                waitSleepTime = std::chrono::microseconds(100);

                actorThreadCount = 4;
                updateThreadCount = 4;
                processingThreadCount = coreCount;
                usePCores = true;
                gpuTaskThreadCount = 4;
                break;
            case Config::AutoTaskQList::Faster:
                taskQTickMS = Config::GetSingleton().GetTaskQTickMS();
                directTaskQ = true;
                divideTaskQ = 0;
                vramSaveMode = true;

                isNoSplitGPU = true;

                waitPreTask = false;
                waitSleepTime = std::chrono::microseconds(500);

                actorThreadCount = 1;
                updateThreadCount = 2;
                processingThreadCount = coreCount;
                usePCores = true;
                gpuTaskThreadCount = 1;
                break;
            case Config::AutoTaskQList::Balanced:
                taskQTickMS = Config::GetSingleton().GetTaskQTickMS();
                directTaskQ = false;
                divideTaskQ = 0;
                vramSaveMode = true;

                isNoSplitGPU = false;

                waitPreTask = false;
                waitSleepTime = std::chrono::microseconds(1000);

                actorThreadCount = 1;
                updateThreadCount = 2;
                processingThreadCount = std::max(1, std::max(coreCount / 2, coreCount - 4)); 
                usePCores = false;
                gpuTaskThreadCount = 1;
                break;
            case Config::AutoTaskQList::BetterPerformance:
                taskQTickMS = Config::GetSingleton().GetTaskQTickMS();
                directTaskQ = false;
                divideTaskQ = 0;
                vramSaveMode = true;

                isNoSplitGPU = false;

                waitPreTask = true;
                waitSleepTime = std::chrono::microseconds(1000);

                actorThreadCount = 1;
                updateThreadCount = 2;
                processingThreadCount = std::max(1, std::min(4, coreCount / 2));
                usePCores = false;
                gpuTaskThreadCount = 1;
                break;
            case Config::AutoTaskQList::BestPerformance:
                taskQTickMS = Config::GetSingleton().GetTaskQTickMS();
                directTaskQ = false;
                divideTaskQ = 1;
                vramSaveMode = true;

                isNoSplitGPU = false;

                waitPreTask = true;
                waitSleepTime = std::chrono::microseconds(1000);

                actorThreadCount = 1;
                updateThreadCount = 1;
                processingThreadCount = 2;
                usePCores = false;
                gpuTaskThreadCount = 1;
                break;
            }
        }

        if (isSecondGPUEnabled)
            vramSaveMode = false;

        std::uint64_t coreMask = 0;
        if (!usePCores)
        {
            std::int32_t eCoreCount = 0;
            DWORD len = 0;
            GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
            std::vector<BYTE> buffer(len);
            if (GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &len))
            {
                BYTE* ptr = buffer.data();
                while (ptr < buffer.data() + len)
                {
                    auto info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
                    if (info->Relationship == RelationProcessorCore)
                    {
                        if (info->Processor.EfficiencyClass > 0)
                        {
                            coreMask += info->Processor.GroupMask[0].Mask;
                            eCoreCount++;
                        }
                    }
                    ptr += info->Size;
                }
            }
            if (coreMask == 0 || eCoreCount == 0)
            {
                coreMask = 0;
                usePCores = true;
            }
            else
            {
                actorThreadCount = std::min(actorThreadCount, eCoreCount);
                memoryManageThreadCount = std::min(memoryManageThreadCount, eCoreCount);
                updateThreadCount = std::min(updateThreadCount, eCoreCount);
                processingThreadCount = eCoreCount;
            }
        }
        gpuTask = std::make_unique<ThreadPool_GPUTaskModule>(gpuTaskThreadCount, coreMask, taskQTickMS, directTaskQ, waitPreTask);
        g_frameEventDispatcher.addListener(gpuTask.get());

        if (isSecondGPUEnabled)
            actorThreadCount = 2;

        actorThreads = std::make_unique<ThreadPool_ParallelModule>(actorThreadCount, coreMask);
        logger::info("set actorThreads {} on {}", actorThreadCount, usePCores ? "all cores" : "all E-cores");

        memoryManageThreads = std::make_unique<ThreadPool_ParallelModule>(memoryManageThreadCount, coreMask);
        logger::info("set memoryManageThreads {} on {}", memoryManageThreadCount, usePCores ? "all cores" : "all E-cores");

        updateThreads = std::make_unique<ThreadPool_ParallelModule>(updateThreadCount, coreMask);
        logger::info("set updateThreads {} on {}", updateThreadCount, usePCores ? "all cores" : "all E-cores");

        processingThreads = std::make_unique<ThreadPool_ParallelModule>(processingThreadCount, coreMask);
        logger::info("set processingThreads {} on {}", processingThreadCount, usePCores ? "all cores" : "all E-cores");

        backGroundHasherThreads = std::make_unique<ThreadPool_ParallelModule>(1u, coreMask);

        weldDistance = std::max(floatPrecision, Config::GetSingleton().GetWeldDistance());
        weldDistanceMult = 1.0f / weldDistance;

        boundaryWeldDistance = std::max(floatPrecision, Config::GetSingleton().GetBoundaryWeldDistance());
        boundaryWeldDistanceMult = 1.0f / boundaryWeldDistance;

        if (Config::GetSingleton().GetRealtimeDetectOnBackGround())
        {
            g_armorAttachEventEventDispatcher.addListener(&ActorVertexHasher::GetSingleton());
            g_facegenNiNodeEventDispatcher.addListener(&ActorVertexHasher::GetSingleton());
            g_actorChangeHeadPartEventDispatcher.addListener(&ActorVertexHasher::GetSingleton());
            ActorVertexHasher::GetSingleton().Init();
        }
    }
}
