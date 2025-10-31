#include "ShaderManager.h"

namespace Mus {
	namespace Shader {
		bool ShaderManager::IsFailedShader(std::string shaderName)
		{
			return failedList.find(lowLetter(shaderName)) != failedList.end();
		}

		bool ShaderManager::SaveCompiledShaderFile(Blob& compiledShader, const std::wstring& shaderFile)
		{
			if (!compiledShader || shaderFile.empty())
				return false;
			std::ofstream file(shaderFile, std::ios::binary);
			if (!file.is_open()) {
				logger::error("Failed to save the compiled shader : {}", wstring2string(shaderFile));
				return false;
			}
			file.clear();
			file.write(static_cast<const char*>(compiledShader->GetBufferPointer()), compiledShader->GetBufferSize());
			file.close();
			logger::info("Save the compiled shader : {}", wstring2string(shaderFile));
			return true;
		}

		bool ShaderManager::LoadComputeShader(ID3D11Device* device, const char* shaderCode, ID3D11ComputeShader** computeShader, Blob& shaderData)
		{
			if (!device || !shaderCode || std::strlen(shaderCode) == 0)
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompile(shaderCode, std::strlen(shaderCode), nullptr, nullptr, nullptr, "CSMain", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shaderData.ReleaseAndGetAddressOf(), &errorBlob);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = device->CreateComputeShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, computeShader);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadComputeShaderFile(ID3D11Device* device, const std::wstring& shaderFile, ID3D11ComputeShader** computeShader, Blob& shaderData)
		{
			if (!device || shaderFile.empty())
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, shaderData.ReleaseAndGetAddressOf(), nullptr);
			if (FAILED(hr))
			{
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = device->CreateComputeShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, computeShader);
			if (FAILED(hr))
			{
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadCompiledComputeShader(ID3D11Device* device, const std::wstring& shaderFile, ID3D11ComputeShader** computeShader)
		{
			if (!device || shaderFile.empty())
				return false;
			std::ifstream file(shaderFile, std::ios::binary);
			if (!file.is_open()) {
				return false;
			}
			file.seekg(0, std::ios::end);
			size_t fileSize = static_cast<size_t>(file.tellg());
			file.seekg(0, std::ios::beg);
			std::vector<char> shaderData(fileSize);
			file.read(shaderData.data(), fileSize);
			file.close();
			HRESULT hr = device->CreateComputeShader(shaderData.data(), fileSize, nullptr, computeShader);
			if (FAILED(hr)) {
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}

		bool ShaderManager::LoadVertexShader(ID3D11Device* device, const char* shaderCode, ID3D11VertexShader** vertexShader, Blob& shaderData)
		{
			if (!device || !shaderCode || std::strlen(shaderCode) == 0)
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompile(shaderCode, std::strlen(shaderCode), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shaderData.ReleaseAndGetAddressOf(), &errorBlob);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = device->CreateVertexShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, vertexShader);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadVertexShaderFile(ID3D11Device* device, const std::wstring& shaderFile, ID3D11VertexShader** vertexShader, Blob& shaderData)
		{
			if (!device || shaderFile.empty())
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, shaderData.ReleaseAndGetAddressOf(), nullptr);
			if (FAILED(hr))
			{
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = device->CreateVertexShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, vertexShader);
			if (FAILED(hr))
			{
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadCompiledVertexShader(ID3D11Device* device, const std::wstring& shaderFile, ID3D11VertexShader** vertexShader)
		{
			if (!device || shaderFile.empty())
				return false;
			std::ifstream file(shaderFile, std::ios::binary);
			if (!file.is_open()) {
				return false;
			}
			file.seekg(0, std::ios::end);
			size_t fileSize = static_cast<size_t>(file.tellg());
			file.seekg(0, std::ios::beg);
			std::vector<char> shaderData(fileSize);
			file.read(shaderData.data(), fileSize);
			file.close();
			HRESULT hr = device->CreateVertexShader(shaderData.data(), fileSize, nullptr, vertexShader);
			if (FAILED(hr)) {
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}

		bool ShaderManager::LoadPixelShader(ID3D11Device* device, const char* shaderCode, ID3D11PixelShader** pixelShader, Blob& shaderData)
		{
			if (!device || !shaderCode || std::strlen(shaderCode) == 0)
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompile(shaderCode, std::strlen(shaderCode), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shaderData.ReleaseAndGetAddressOf(), &errorBlob);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = device->CreatePixelShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, pixelShader);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadPixelShaderFile(ID3D11Device* device, const std::wstring& shaderFile, ID3D11PixelShader** pixelShader, Blob& shaderData)
		{
			if (!device || shaderFile.empty())
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, shaderData.ReleaseAndGetAddressOf(), nullptr);
			if (FAILED(hr))
			{
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = device->CreatePixelShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, pixelShader);
			if (FAILED(hr))
			{
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadCompiledPixelShader(ID3D11Device* device, const std::wstring& shaderFile, ID3D11PixelShader** pixelShader)
		{
			if (!device || shaderFile.empty())
				return false;
			std::ifstream file(shaderFile, std::ios::binary);
			if (!file.is_open()) {
				return false;
			}
			file.seekg(0, std::ios::end);
			size_t fileSize = static_cast<size_t>(file.tellg());
			file.seekg(0, std::ios::beg);
			std::vector<char> shaderData(fileSize);
			file.read(shaderData.data(), fileSize);
			file.close();
			HRESULT hr = device->CreatePixelShader(shaderData.data(), fileSize, nullptr, pixelShader);
			if (FAILED(hr)) {
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}

		std::wstring ShaderManager::GetShaderFilePath(std::string shaderName, bool compiled, ShaderType shaderType)
		{
			std::string file = "Data\\SKSE\\Plugins\\MuDynamicNormalMap\\Shader\\";
			if (compiled)
			{
				file += "Compiled\\";

				switch (shaderType)
				{
				case ShaderType::Compute:
					file += "CS_";
					break;
				case ShaderType::Vertex:
					file += "VS_";
					break;
				case ShaderType::Pixel:
					file += "PS_";
					break;
				}
			}
			file += shaderName;
			std::string file_ = file;
			if (compiled)
				file_ += ".cso";
			else
				file_ += ".hlsl";
			if (!compiled)
			{
				if (!IsExistFileInStream(file_))
				{
					logger::critical("Failed to get shader file");
					return L"";
				}
			}
			return string2wstring(file_);
		}

		ShaderManager::ComputeShader ShaderManager::CreateComputeShader(ID3D11Device* device, std::string shaderName)
		{
			std::string shaderName_ = lowLetter(shaderName);
			if (auto found = computeShaders.find(shaderName_); found != computeShaders.end())
				return found->second;
			if (failedList.find(shaderName_) != failedList.end())
				return nullptr;
			ComputeShader newShader = nullptr;
			Blob csBlob = nullptr;
			if (LoadComputeShaderFile(device, GetShaderFilePath(shaderName, false, ShaderType::Compute), &newShader, csBlob))
			{
				computeShaders[shaderName_] = newShader;
				logger::info("Compile shader done : {}", shaderName);
				SaveCompiledShaderFile(csBlob, GetShaderFilePath(shaderName, true, ShaderType::Compute));
				csBlobs[shaderName_] = csBlob;
				return newShader;
			}
			logger::warn("Failed to compile shader for {}. so try to load latest compiled shader...", shaderName);
			if (LoadCompiledComputeShader(device, GetShaderFilePath(shaderName, true, ShaderType::Compute), &newShader))
			{
				computeShaders[shaderName_] = newShader;
				logger::info("Loaded latest compiled shader : {}", shaderName);
				D3DReadFileToBlob(GetShaderFilePath(shaderName, true, ShaderType::Compute).c_str(), &csBlob);
				csBlobs[shaderName_] = csBlob;
				return newShader;
			}
			std::string FailedShaderCompile = "";
			if (FailedShaderCompile.length() == 0)
			{
				FailedShaderCompile = "MuDynamicNormalMap - Warning!";
				FailedShaderCompile += "\n\nFailed to Compile shader!";
				FailedShaderCompile += "\nPlease update graphic driver";
			}
			else
				FailedShaderCompile = ReplaceNewlineEscapes(FailedShaderCompile);
			RE::DebugMessageBox(FailedShaderCompile.c_str());
			failedList.emplace(shaderName);
			return nullptr;
		}
		ShaderManager::VertexShader ShaderManager::CreateVertexShader(ID3D11Device* device, std::string shaderName)
		{
			std::string shaderName_ = lowLetter(shaderName);
			if (auto found = vertexShaders.find(shaderName_); found != vertexShaders.end())
				return found->second;
			if (failedList.find(shaderName_) != failedList.end())
				return nullptr;
			VertexShader newShader = nullptr;
			Blob vsBlob = nullptr;
			if (LoadVertexShaderFile(device, GetShaderFilePath(shaderName, false, ShaderType::Vertex), &newShader, vsBlob))
			{
				vertexShaders[shaderName_] = newShader;
				logger::info("Compile shader done : {}", shaderName);
				SaveCompiledShaderFile(vsBlob, GetShaderFilePath(shaderName, true, ShaderType::Vertex));
				vsBlobs[shaderName_] = vsBlob;
				return newShader;
			}
			logger::warn("Failed to compile shader for {}. so try to load latest compiled shader...", shaderName);
			if (LoadCompiledVertexShader(device, GetShaderFilePath(shaderName, true, ShaderType::Vertex), &newShader))
			{
				vertexShaders[shaderName_] = newShader;
				logger::info("Loaded latest compiled shader : {}", shaderName);
				D3DReadFileToBlob(GetShaderFilePath(shaderName, true, ShaderType::Vertex).c_str(), &vsBlob);
				vsBlobs[shaderName_] = vsBlob;
				return newShader;
			}
			std::string FailedShaderCompile = "";
			if (FailedShaderCompile.length() == 0)
			{
				FailedShaderCompile = "MuDynamicNormalMap - Warning!";
				FailedShaderCompile += "\n\nFailed to Compile shader!";
				FailedShaderCompile += "\nPlease update graphic driver";
			}
			else
				FailedShaderCompile = ReplaceNewlineEscapes(FailedShaderCompile);
			RE::DebugMessageBox(FailedShaderCompile.c_str());
			failedList.emplace(shaderName);
			return nullptr;
		}
		ShaderManager::PixelShader ShaderManager::CreatePixelShader(ID3D11Device* device, std::string shaderName)
		{
			std::string shaderName_ = lowLetter(shaderName);
			if (auto found = pixelShaders.find(shaderName_); found != pixelShaders.end())
				return found->second;
			if (failedList.find(shaderName_) != failedList.end())
				return nullptr;
			PixelShader newShader = nullptr;
			Blob psBlob = nullptr;
			if (LoadPixelShaderFile(device, GetShaderFilePath(shaderName, false, ShaderType::Pixel), &newShader, psBlob))
			{
				pixelShaders[shaderName_] = newShader;
				logger::info("Compile shader done : {}", shaderName);
				SaveCompiledShaderFile(psBlob, GetShaderFilePath(shaderName, true, ShaderType::Pixel));
				psBlobs[shaderName_] = psBlob;
				return newShader;
			}
			logger::warn("Failed to compile shader for {}. so try to load latest compiled shader...", shaderName);
			if (LoadCompiledPixelShader(device, GetShaderFilePath(shaderName, true, ShaderType::Pixel), &newShader))
			{
				pixelShaders[shaderName_] = newShader;
				logger::info("Loaded latest compiled shader : {}", shaderName);
				D3DReadFileToBlob(GetShaderFilePath(shaderName, true, ShaderType::Pixel).c_str(), &psBlob);
				psBlobs[shaderName_] = psBlob;
				return newShader;
			}
			std::string FailedShaderCompile = "";
			if (FailedShaderCompile.length() == 0)
			{
				FailedShaderCompile = "MuDynamicNormalMap - Warning!";
				FailedShaderCompile += "\n\nFailed to Compile shader!";
				FailedShaderCompile += "\nPlease update graphic driver";
			}
			else
				FailedShaderCompile = ReplaceNewlineEscapes(FailedShaderCompile);
			RE::DebugMessageBox(FailedShaderCompile.c_str());
			failedList.emplace(shaderName);
			return nullptr;
		}

		ShaderManager::ComputeShader ShaderManager::GetComputeShader(ID3D11Device* device, std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = computeShaders.find(shaderName); found != computeShaders.end())
				return found->second;
			return CreateComputeShader(device, shaderName);
		}
		ShaderManager::VertexShader ShaderManager::GetVertexShader(ID3D11Device* device, std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = vertexShaders.find(shaderName); found != vertexShaders.end())
				return found->second;
			return CreateVertexShader(device, shaderName);
		}
		ShaderManager::PixelShader ShaderManager::GetPixelShader(ID3D11Device* device, std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = pixelShaders.find(shaderName); found != pixelShaders.end())
				return found->second;
			return CreatePixelShader(device, shaderName);
		}

		ShaderManager::Blob ShaderManager::GetComputeShaderBlob(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = csBlobs.find(shaderName); found != csBlobs.end())
				return found->second;
			return nullptr;
		}
		ShaderManager::Blob ShaderManager::GetVertexShaderBlob(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = vsBlobs.find(shaderName); found != vsBlobs.end())
				return found->second;
			return nullptr;
		}
		ShaderManager::Blob ShaderManager::GetPixelShaderBlob(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = psBlobs.find(shaderName); found != psBlobs.end())
				return found->second;
			return nullptr;
		}

		ID3D11DeviceContext* ShaderManager::GetContext()
		{
			if (!context_)
				context_ = reinterpret_cast<ID3D11DeviceContext*>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context);
			return context_;
		}
		ID3D11Device* ShaderManager::GetDevice()
		{
			if (!device_)
			{
				if (GetContext())
				{
					ShaderContextLock();
					GetContext()->GetDevice(&device_);
					ShaderContextUnlock();
				}
			}
			return device_;
		}
		bool ShaderManager::CreateDeviceContextWithSecondGPUImpl()
		{
			HRESULT hr;

			INT gpuIndex = Config::GetSingleton().GetGPUDeviceIndex();
			if (gpuIndex == 0)
			{
				logger::info("Use main GPU for compute");
				return false;
			}

			Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
			hr = GetDevice()->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
			if (FAILED(hr))
			{
				logger::error("Unable to get Main GPU device index");
				return false;
			}

			Microsoft::WRL::ComPtr<IDXGIAdapter> mainGPUDevice;
			hr = dxgiDevice->GetAdapter(&mainGPUDevice);
			if (FAILED(hr))
			{
				logger::error("Unable to get Main GPU device index");
				return false;
			}

			DXGI_ADAPTER_DESC mainGPUDesc = {};
			mainGPUDevice->GetDesc(&mainGPUDesc);

			const LUID mainLuid = mainGPUDesc.AdapterLuid;

			Microsoft::WRL::ComPtr<IDXCoreAdapterFactory> factory;
			hr = DXCoreCreateAdapterFactory(IID_PPV_ARGS(&factory));
			if (FAILED(hr))
			{
				logger::error("Failed to create IDXCoreAdapterFactory : {}", hr);
				return false;
			}

			Microsoft::WRL::ComPtr<IDXCoreAdapterList> gpuList;
			constexpr GUID gpuFilter = { 0x8c47866b, 0x7583, 0x450d, 0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b }; //DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS
			hr = factory->CreateAdapterList(1u, &gpuFilter, gpuList.GetAddressOf());
			if (FAILED(hr)) {
				logger::error("Failed to create DXCoreAdapterList : {}", hr);
				return false;
			}

			std::uint32_t gpuCount = gpuList->GetAdapterCount();
			if (gpuIndex > 0 && gpuIndex >= gpuCount)
			{
				logger::error("Invalid GPU Index. so use main GPU for compute");
				return false;
			}

			Microsoft::WRL::ComPtr<IDXCoreAdapter> gpuDevice;
			std::uint32_t hardwareGPUIndex = 1;
			for (std::uint32_t i = 0; i < gpuCount; i++)
			{
				hr = gpuList->GetAdapter(i, gpuDevice.ReleaseAndGetAddressOf());
				if (FAILED(hr))
				{
					logger::warn("Unable to get GPU device({})", i);
					continue;
				}
				bool isHardware = false;
				gpuDevice->GetProperty(DXCoreAdapterProperty::IsHardware, sizeof(isHardware), &isHardware);
				if (!isHardware)
				{
					logger::warn("Not found hardware GPU({})", i);
					continue;
				}

				char description[128] = {};
				gpuDevice->GetProperty(DXCoreAdapterProperty::DriverDescription, sizeof(description), description);

				LUID gpuLuid;
				gpuDevice->GetProperty(DXCoreAdapterProperty::InstanceLuid, sizeof(LUID), &gpuLuid);
				if (gpuLuid.HighPart == mainLuid.HighPart && gpuLuid.LowPart == mainLuid.LowPart)
				{
					logger::warn("Found a hardware GPU device : {}({}). but the main GPU device already being used for Skyrim renderer", description, 0);
					continue;
				}

				if (gpuIndex > 0 && gpuIndex != hardwareGPUIndex)
				{
					logger::warn("Found a hardware GPU device : {}({}). but doesn't match GPUDeviceIndex", description, hardwareGPUIndex);
					hardwareGPUIndex++;
					continue;
				}
				logger::info("Found a hardware GPU device : {}({})", description, hardwareGPUIndex);

				gpuIndex = i;
				break;
			}
			if (gpuIndex < 0)
			{
				logger::error("There is no secondary GPU device. so use main GPU for compute");
				return false;
			}

			char description[128] = {};
			gpuDevice->GetProperty(DXCoreAdapterProperty::DriverDescription, sizeof(description), description);
			logger::info("Selected secondary GPU for compute : {}", description);

			LUID gpuLuid;
			gpuDevice->GetProperty(DXCoreAdapterProperty::InstanceLuid, sizeof(gpuLuid), &gpuLuid);

			Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
			hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
			if (FAILED(hr)) {
				logger::error("Failed to create DXGIFactory4 : {}", hr);
				return false;
			}

			Microsoft::WRL::ComPtr<IDXGIAdapter> gpu;
			hr = dxgiFactory->EnumAdapterByLuid(gpuLuid, IID_PPV_ARGS(&gpu));
			if (FAILED(hr)) {
				logger::error("Failed to get DXGIAdapter with secondary GPU : {}", hr);
				return false;
			}

			D3D_FEATURE_LEVEL featureLevels[] = {
				D3D_FEATURE_LEVEL_11_0
			};
			D3D_FEATURE_LEVEL featureLevelCreated;
			hr = D3D11CreateDevice(
				gpu.Get(),
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				D3D11_CREATE_DEVICE_BGRA_SUPPORT,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				secondDevice_.ReleaseAndGetAddressOf(),
				&featureLevelCreated,
				secondContext_.ReleaseAndGetAddressOf()
			);
			if (FAILED(hr)) {
				logger::error("The GPU device ({}) does not support DirectX 11: {}", description, hr);
				return false;
			}

			logger::info("Create D3D11 device and context with secondary GPU : {}", description);
			return true;
		}
		bool ShaderManager::CreateDeviceContextWithSecondGPU()
		{
			if (!CreateDeviceContextWithSecondGPUImpl())
			{
				secondDevice_.Reset();
				secondContext_.Reset();
			}
			return true;
		}
		ID3D11DeviceContext* ShaderManager::GetSecondContext()
		{
			if (isSearchSecondGPU)
			{
				isSearchSecondGPU = false;
				CreateDeviceContextWithSecondGPU();
			}
			return secondContext_ ? secondContext_.Get() : nullptr;
		}
		ID3D11Device* ShaderManager::GetSecondDevice()
		{
			if(isSearchSecondGPU)
			{
				isSearchSecondGPU = false;
				CreateDeviceContextWithSecondGPU();
			}
			return secondDevice_ ? secondDevice_.Get() : nullptr;
		}
		bool ShaderManager::IsValidSecondGPU()
		{
			return GetSecondContext() && GetSecondDevice();
		}

		RE::NiPointer<RE::NiSourceTexture> TextureLoadManager::GetNiSourceTexture(std::string filePath, std::string name)
		{
			auto device = ShaderManager::GetSingleton().GetDevice();
			if (!device)
				return nullptr;

			D3D11_TEXTURE2D_DESC texDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
			GetTexture2D(filePath, texDesc, srvDesc, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, false, texture2d);
			if (!texture2d)
			{
				logger::error("Failed to get texture 2d for NiTexture ({})", filePath);
				return nullptr;
			}
			texture2d->GetDesc(&texDesc);
			srvDesc.Format = texDesc.Format;
			srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;
			HRESULT hr = device->CreateShaderResourceView(texture2d.Get(), &srvDesc, &textureSRV);
			if (FAILED(hr))
			{
				logger::error("Failed to create ShaderResourceView for NiTexture ({})", hr);
				return nullptr;
			}
			RE::NiPointer<RE::NiSourceTexture> output;
			auto result = CreateSourceTexture(name, output);
			if (result == -1)
			{
				logger::critical("Failed to create NiTexture");
				return nullptr;
			}
			else if (result == 0)
			{
				auto oldTexture = output->rendererTexture->texture;
				auto oldResource = output->rendererTexture->resourceView;
				output->rendererTexture->texture = texture2d.Detach();
				output->rendererTexture->resourceView = textureSRV.Detach();
				if (oldTexture)
					oldTexture->Release();
				if (oldResource)
					oldResource->Release();
			}
			else if (result == 1)
			{
				RE::BSGraphics::Texture* newRendererTexture = new RE::BSGraphics::Texture();
				newRendererTexture->texture = texture2d.Detach();
				newRendererTexture->resourceView = textureSRV.Detach();
				output->rendererTexture = newRendererTexture;
			}
			return output;
		}

		std::int8_t TextureLoadManager::IsCompressFormat(DXGI_FORMAT format)
		{
			switch (format)
			{
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
				return 1;
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC7_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
				return 2;
			default:
				return -1;
			}
		}

		std::int8_t TextureLoadManager::CreateNiTexture(std::string name, Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstSRV, RE::NiPointer<RE::NiSourceTexture>& output)
		{
			RE::NiPointer<RE::NiSourceTexture> newTexture;
			auto result = TextureLoadManager::CreateSourceTexture(name, newTexture);
			if (result == -1 || !newTexture)
			{
				logger::critical("Failed to create NiTexture");
				return result;
			}
			else if (result == 0)
			{
				auto oldTexture = newTexture->rendererTexture->texture;
				auto oldResource = newTexture->rendererTexture->resourceView;
				newTexture->rendererTexture->texture = dstTex.Detach();
				newTexture->rendererTexture->resourceView = dstSRV.Detach();
				if (oldTexture)
					oldTexture->Release();
				if (oldResource)
					oldResource->Release();
			}
			else if (result == 1)
			{
				RE::BSGraphics::Texture* newRendererTexture = new RE::BSGraphics::Texture();
				newRendererTexture->texture = dstTex.Detach();
				newRendererTexture->resourceView = dstSRV.Detach();
				newTexture->rendererTexture = newRendererTexture;
			}
			output = newTexture;
			return result;
		}

		Microsoft::WRL::ComPtr<ID3D11Texture2D> TextureLoadManager::GetNiTexture(std::string name)
		{
			auto found = niTextures.find(name);
			if (found != niTextures.end())
			{
				if (found->second && found->second->rendererTexture)
					return found->second->rendererTexture->texture;
			}
			return nullptr;
		}

		void TextureLoadManager::ReleaseNiTexture(std::string name)
		{
			auto found = niTextures.find(name);
			if (found != niTextures.end())
			{
				if (found->second)
				{
					auto oldTexture = found->second->rendererTexture->texture;
					auto oldResource = found->second->rendererTexture->resourceView;
					found->second->rendererTexture->texture = nullptr;
					found->second->rendererTexture->resourceView = nullptr;
					if (oldTexture)
						oldTexture->Release();
					if (oldResource)
						oldResource->Release();
				}
			}
		}

		bool TextureLoadManager::PrintTexture(std::string filePath, ID3D11Texture2D* texture)
		{
			if (filePath.empty() || !texture)
				return false;

			auto device = ShaderManager::GetSingleton().GetDevice();
			auto context = ShaderManager::GetSingleton().GetContext();
			if (!device || !context)
				return false;

			DirectX::ScratchImage image;
			ShaderManager::GetSingleton().ShaderContextLock();
			HRESULT hr = DirectX::CaptureTexture(device, context, texture, image);
			ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to capture texture ({})", hr);
				return false;
			}

			hr = DirectX::SaveToDDSFile(*image.GetImage(0, 0, 0), DirectX::DDS_FLAGS_NONE, string2wstring(filePath).c_str());
			if (FAILED(hr))
			{
				logger::error("Failed to save texture to file ({})", hr);
				return false;
			}
			return true;
		}
		
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			filePath = stringRemoveStarts(filePath, "Data\\");
			if (!stringStartsWith(filePath, "textures"))
				filePath = "Textures\\" + filePath;

			if (!IsExistFileInStream(filePath, ExistType::textures))
			{
				logger::error("Failed to load texture file : {}", filePath);
				return false;
			}
			RE::NiPointer<RE::NiSourceTexture> sourceTexture;
			LoadTexture(filePath.c_str(), 1, sourceTexture, false);
			if (!sourceTexture || !sourceTexture->rendererTexture || !sourceTexture->rendererTexture->resourceView)
			{
				logger::error("Failed to load texture file : {}", filePath);
				return false;
			}
			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			sourceTexture->rendererTexture->resourceView->GetDesc(&srvDesc);
			sourceTexture->rendererTexture->resourceView->GetResource(&resource);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
			HRESULT hr = resource.As(&texture2D);
			if (FAILED(hr))
			{
				logger::error("Failed to load texture resource ({})", hr);
				return false;
			}
			texture2D->GetDesc(&textureDesc);
			if (newFormat == DXGI_FORMAT_UNKNOWN && newWidth == 0 && newHeight == 0)
			{
				output = texture2D;
			}
			else
			{
				ConvertTexture(ShaderManager::GetSingleton().GetDevice(), ShaderManager::GetSingleton().GetContext(), texture2D, newFormat, newWidth, newHeight, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_FANT, cpuReadable, output);
			}
			if (!output)
			{
				logger::error("Failed to convert texture : {}", filePath);
				return false;
			}
			return true;
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			return GetTexture2D(filePath, textureDesc, srvDesc, newFormat, 0, 0, cpuReadable, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_TEXTURE2D_DESC tmpTexDesc;
			return GetTexture2D(filePath, tmpTexDesc, srvDesc, newFormat, cpuReadable, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC tmpSRVDesc;
			return GetTexture2D(filePath, textureDesc, tmpSRVDesc, newFormat, cpuReadable, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_TEXTURE2D_DESC tmpTexDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC tmpSRVDesc;
			return GetTexture2D(filePath, tmpTexDesc, tmpSRVDesc, newFormat, cpuReadable, output);
		}

		bool TextureLoadManager::GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			if (!device)
				return false;
			if (!stringEndsWith(filePath, ".dds"))
				return false;
			if (!stringStartsWith(filePath, "Textures"))
				filePath = "Textures\\" + filePath;
			if (!stringStartsWith(filePath, "Data"))
				filePath = "Data\\" + filePath;
			DirectX::ScratchImage image;
			HRESULT hr = LoadFromDDSFile(string2wstring(filePath).c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
			if (FAILED(hr)) {
				logger::error("Failed to get texture from {} file", filePath);
				return false;
			}
			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			if (!ConvertD3D11(device, image, cpuReadable, resource))
			{
				logger::error("Failed to get texture from {} file", filePath);
				return false;
			}
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
			hr = resource.As(&texture2D);
			if (FAILED(hr)) {
				logger::error("Failed to get texture resource from {} file", filePath);
				return false;
			}
			if (newFormat == DXGI_FORMAT_UNKNOWN && newWidth == 0 && newHeight == 0)
			{
				output = texture2D;
			}
			else
			{
				ConvertTexture(device, context, texture2D, newFormat, newWidth, newHeight, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_FANT, cpuReadable, output);
			}
			if (!output)
			{
				logger::error("Failed to convert texture : {}", filePath);
				return false;
			}
			output->GetDesc(&textureDesc);
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
			return true;
		}
		bool TextureLoadManager::GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			return GetTextureFromFile(device, context, filePath, textureDesc, srvDesc, newFormat, 0, 0, cpuReadable, output);
		}
		bool TextureLoadManager::GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_TEXTURE2D_DESC tmpTexDesc;
			return GetTextureFromFile(device, context, filePath, tmpTexDesc, srvDesc, newFormat, cpuReadable, output);
		}
		bool TextureLoadManager::GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC tmpSRVDesc;
			return GetTextureFromFile(device, context, filePath, textureDesc, tmpSRVDesc, newFormat, cpuReadable, output);
		}
		bool TextureLoadManager::GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_TEXTURE2D_DESC tmpTexDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC tmpSRVDesc;
			return GetTextureFromFile(device, context, filePath, tmpTexDesc, tmpSRVDesc, newFormat, cpuReadable, output);
		}

		bool TextureLoadManager::UpdateTexture(std::string filePath)
		{
			filePath = stringRemoveStarts(filePath, "Data\\");
			if (!stringStartsWith(filePath, "textures"))
				filePath = "Textures\\" + filePath;
			filePath = FixPath(filePath);

			if (!UpdateNiTexture(filePath))
				return false;
			logger::info("Update NiTexture Done : {}", filePath);
			return true;
		}
		bool TextureLoadManager::UpdateNiTexture(std::string filePath)
		{
			if (!IsExistFile(filePath, ExistType::textures, true))
				return true;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			if (!GetTextureFromFile(ShaderManager::GetSingleton().GetDevice(), ShaderManager::GetSingleton().GetContext(), filePath, srvDesc, DXGI_FORMAT::DXGI_FORMAT_UNKNOWN, false, texture))
			{
				logger::error("Failed to load texture file : {}", filePath);
				return false;
			}
			HRESULT hr = ShaderManager::GetSingleton().GetDevice()->CreateShaderResourceView(texture.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to create ShaderResourceView for NiTexture ({})", hr);
				return false;
			}

			RE::NiPointer<RE::NiSourceTexture> sourceTexture;
			LoadTexture(filePath.c_str(), 1, sourceTexture, false);
			if (!sourceTexture || !sourceTexture->rendererTexture || !sourceTexture->rendererTexture->resourceView)
			{
				logger::error("Failed to load texture file : {}", filePath);
				return false;
			}

			auto oldTexture = sourceTexture->rendererTexture->texture;
			auto oldResource = sourceTexture->rendererTexture->resourceView;
			sourceTexture->rendererTexture->texture = texture.Detach();
			sourceTexture->rendererTexture->resourceView = srv.Detach();
			if (oldTexture)
				oldTexture->Release();
			if (oldResource)
				oldResource->Release();
			return true;
		}
		bool TextureLoadManager::ConvertTexture(ID3D11Device* device, ID3D11DeviceContext* context, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, DirectX::TEX_FILTER_FLAGS resizeFilter, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			if (!texture || !device || !context)
				return false;

			D3D11_TEXTURE2D_DESC textureDesc;
			texture->GetDesc(&textureDesc);

			const bool isSecondGPU = Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context);
			auto ShaderLock = [isSecondGPU]() {
				if (isSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
			};
			auto ShaderUnlock = [&]() {
				if (isSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			};

			// decoding texture
			DirectX::ScratchImage image;
			ShaderLock();
			HRESULT hr = DirectX::CaptureTexture(device, context, texture.Get(), image);
			ShaderUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to decoding texture ({})", hr);
				return false;
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> tmpResource;
			bool convertResult = false;
			if (newFormat != DXGI_FORMAT_UNKNOWN && textureDesc.Format != newFormat)
			{
				// convert format
				DirectX::ScratchImage convertedImage;
				hr = DirectX::Decompress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, convertedImage);
				if (FAILED(hr))
				{
					hr = DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, DirectX::TEX_FILTER_FANT, 0.0f, convertedImage);
					if (FAILED(hr))
					{
						logger::error("Failed to convert texture {} to {} ({})", magic_enum::enum_name(textureDesc.Format).data(), magic_enum::enum_name(newFormat).data(), hr);
						return false;
					}
				}

				if ((newWidth > 0 && newHeight > 0) && (textureDesc.Width != newWidth || textureDesc.Height != newHeight))
				{
					DirectX::ScratchImage resize;
					hr = DirectX::Resize(convertedImage.GetImages(), convertedImage.GetImageCount(), convertedImage.GetMetadata(), newWidth, newHeight, resizeFilter, resize);
					if (FAILED(hr))
					{
						logger::error("Failed to resize texture ({})", hr);
						return false;
					}
					convertResult = ConvertD3D11(device, resize, cpuReadable, tmpResource);
				}
				else
				{
					convertResult = ConvertD3D11(device, convertedImage, cpuReadable, tmpResource);
				}
			}
			else
			{
				if ((newWidth > 0 && newHeight > 0) && (textureDesc.Width != newWidth || textureDesc.Height != newHeight))
				{
					DirectX::ScratchImage resize;
					hr = DirectX::Resize(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newWidth, newHeight, resizeFilter, resize);
					if (FAILED(hr))
					{
						logger::error("Failed to resize texture ({})", hr);
						return false;
					}
					convertResult = ConvertD3D11(device, resize, cpuReadable, tmpResource);
				}
				else
				{
					convertResult = ConvertD3D11(device, image, cpuReadable, tmpResource);
				}
			}
			if (!convertResult || !tmpResource)
			{
				logger::error("Failed to convert texture to d3d11 resource");
				return false;
			}

			Microsoft::WRL::ComPtr<ID3D11Texture2D> tmpTexture;
			hr = tmpResource.As(&tmpTexture);
			if (FAILED(hr))
			{
				logger::error("Failed to convert texture to d3d11 resource ({})", hr);
				return false;
			}
			output = tmpTexture;
			return true;
		}
		bool TextureLoadManager::ConvertD3D11(ID3D11Device* device, DirectX::ScratchImage& image, bool cpuReadabl, Microsoft::WRL::ComPtr<ID3D11Resource>& output)
		{
			// convert texture to d3d11 texture
			if (!device)
				return false;
			HRESULT hr;
			if (cpuReadabl)
				hr = DirectX::CreateTextureEx(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), 
											  D3D11_USAGE_STAGING, 
											  0, 
											  D3D11_CPU_ACCESS_READ, 
											  0, 
											  DirectX::CREATETEX_FLAGS::CREATETEX_DEFAULT, 
											  output.ReleaseAndGetAddressOf());
			else
				hr = DirectX::CreateTextureEx(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), 
											  D3D11_USAGE_DEFAULT, 
											  D3D11_BIND_SHADER_RESOURCE, 
											  0, 
											  0, 
											  DirectX::CREATETEX_FLAGS::CREATETEX_DEFAULT, 
											  output.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to convert texture to d3d11 ({})", hr);
				return false;
			}
			return true;
		}
		bool TextureLoadManager::CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, DXGI_FORMAT newFormat, bool cpuReadable, std::uint8_t quality, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut)
		{
			if (!texInOut || !device || !context)
				return false;

			auto formatType = IsCompressFormat(newFormat);
			if (formatType < 0)
				return false;

			bool isSecondGPU = ShaderManager::GetSingleton().IsSecondGPUResource(context);
			auto ShaderLock = [isSecondGPU]() {
				if (isSecondGPU)
					ShaderManager::GetSingleton().ShaderSecondContextLock();
				else
					ShaderManager::GetSingleton().ShaderContextLock();
			};
			auto ShaderUnlock = [isSecondGPU]() {
				if (isSecondGPU)
					ShaderManager::GetSingleton().ShaderSecondContextUnlock();
				else
					ShaderManager::GetSingleton().ShaderContextUnlock();
			};

			// decoding texture
			DirectX::ScratchImage image;
			ShaderLock();
			HRESULT hr = DirectX::CaptureTexture(device, context, texInOut.Get(), image);
			ShaderUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to decoding texture ({})", hr);
				return false;
			}

			DirectX::ScratchImage compressedImage;
			DirectX::TEX_COMPRESS_FLAGS flags = DirectX::TEX_COMPRESS_DEFAULT;
			if (formatType == 1) //CPU format
			{
				flags |= DirectX::TEX_COMPRESS_DITHER | DirectX::TEX_COMPRESS_PARALLEL;
				hr = DirectX::Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, flags, 1.0f, compressedImage);
				if (FAILED(hr))
				{
					logger::error("Failed to compress texture to {} ({})", magic_enum::enum_name(newFormat).data(), hr);
					return false;
				}
			}
			else if (formatType == 2) //GPU format
			{
				if (quality == 0)
				{
					flags |= DirectX::TEX_COMPRESS_BC7_QUICK;
				}
				else if (quality == 1)
				{
					// default
				}
				else if (quality == 2)
				{
					flags |= DirectX::TEX_COMPRESS_BC7_USE_3SUBSETS;
				}

				ShaderLock();
				hr = DirectX::Compress(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, flags, 1.0f, compressedImage);
				ShaderUnlock();
				if (FAILED(hr))
				{
					logger::error("Failed to compress texture to {} ({})", magic_enum::enum_name(newFormat).data(), hr);
					return false;
				}
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> tmpResource;
			if (!ConvertD3D11(device, compressedImage, cpuReadable, tmpResource))
			{
				logger::error("Failed to convert texture to d3d11 resource");
				return false;
			}

			Microsoft::WRL::ComPtr<ID3D11Texture2D> tmpTexture;
			hr = tmpResource.As(&tmpTexture);
			if (FAILED(hr))
			{
				logger::error("Failed to get compress texture from the resource({})", hr);
				return false;
			}
			texInOut = tmpTexture;
			return true;
		}
	}
}
