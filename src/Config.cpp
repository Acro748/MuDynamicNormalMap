#include "Config.h"

using namespace Mus;

namespace Mus {
    bool Config::LoadLogging()
    {
        std::string configPath = GetRuntimeSKSEDirectory();
        configPath += "MuDynamicTextureTool.ini";

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
        configPath += "MuDynamicTextureTool.ini";

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
				else if (variableName == "BakeCoreLimit")
				{
					BakeCoreLimit = GetIntValue(variableValue);
				}
				else if (variableName == "TextureResize")
				{
					TextureResize = GetFloatValue(variableValue);
				}
				else if (variableName == "IgnoreTextureSize")
				{
					IgnoreTextureSize = GetBoolValue(variableValue);
				}
				else if (variableName == "NormalmapBakeDelayTick")
				{
                    NormalmapBakeDelayTick = GetUIntValue(variableValue);
				}
			}
        }

        std::uint32_t cores = std::thread::hardware_concurrency();
        logger::info("Detected cores : {}", cores);
        auto orgBakeCoreLimit = BakeCoreLimit;
		if (BakeCoreLimit < 0)
		{
			BakeCoreLimit = (std::max)(1, static_cast<std::int32_t>(cores / (1 - BakeCoreLimit)));
            logger::info("BakeCoreLimit is {}. so set core limit {} / {} = {}", orgBakeCoreLimit, cores, (1 - orgBakeCoreLimit), BakeCoreLimit);
		}
		else if (BakeCoreLimit == 0)
		{
			BakeCoreLimit = static_cast<std::int32_t>(cores);
            logger::info("BakeCoreLimit is {}. so set core limit max {}", orgBakeCoreLimit, BakeCoreLimit);
		}
		else //if (BakeCoreLimit > 0)
        {
            BakeCoreLimit = (std::min)(BakeCoreLimit, static_cast<std::int32_t>(cores));
            logger::info("BakeCoreLimit is {}. so set core limit {}", orgBakeCoreLimit, BakeCoreLimit);
        }

        return true;
    }
}

