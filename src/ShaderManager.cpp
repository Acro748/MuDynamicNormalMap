#include "ShaderManager.h"

namespace Mus {
	namespace Shader {
		bool ShaderManager::IsFailedShader(std::string shaderName)
		{
			return Failed.find(lowLetter(shaderName)) != Failed.end();
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

		bool ShaderManager::LoadComputeShader(const char* shaderCode, ID3D11ComputeShader** computeShader, Blob& shaderData)
		{
			if (!GetDevice() || !shaderCode || std::strlen(shaderCode) == 0)
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompile(shaderCode, std::strlen(shaderCode), nullptr, nullptr, nullptr, "CSMain", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shaderData.ReleaseAndGetAddressOf(), &errorBlob);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = GetDevice()->CreateComputeShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, computeShader);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadComputeShaderFile(const std::wstring& shaderFile, ID3D11ComputeShader** computeShader, Blob& shaderData)
		{
			if (!GetDevice() || shaderFile.empty())
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "CSMain", "cs_5_0", 0, 0, shaderData.ReleaseAndGetAddressOf(), nullptr);
			if (FAILED(hr))
			{
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = GetDevice()->CreateComputeShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, computeShader);
			if (FAILED(hr))
			{
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadCompiledComputeShader(const std::wstring& shaderFile, ID3D11ComputeShader** computeShader)
		{
			if (!GetDevice() || shaderFile.empty())
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
			HRESULT hr = GetDevice()->CreateComputeShader(shaderData.data(), fileSize, nullptr, computeShader);
			if (FAILED(hr)) {
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}

		bool ShaderManager::LoadVertexShader(const char* shaderCode, ID3D11VertexShader** vertexShader, Blob& shaderData)
		{
			if (!GetDevice() || !shaderCode || std::strlen(shaderCode) == 0)
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompile(shaderCode, std::strlen(shaderCode), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shaderData.ReleaseAndGetAddressOf(), &errorBlob);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = GetDevice()->CreateVertexShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, vertexShader);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadVertexShaderFile(const std::wstring& shaderFile, ID3D11VertexShader** vertexShader, Blob& shaderData)
		{
			if (!GetDevice() || shaderFile.empty())
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, shaderData.ReleaseAndGetAddressOf(), nullptr);
			if (FAILED(hr))
			{
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = GetDevice()->CreateVertexShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, vertexShader);
			if (FAILED(hr))
			{
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadCompiledVertexShader(const std::wstring& shaderFile, ID3D11VertexShader** vertexShader)
		{
			if (!GetDevice() || shaderFile.empty())
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
			HRESULT hr = GetDevice()->CreateVertexShader(shaderData.data(), fileSize, nullptr, vertexShader);
			if (FAILED(hr)) {
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}

		bool ShaderManager::LoadPixelShader(const char* shaderCode, ID3D11PixelShader** pixelShader, Blob& shaderData)
		{
			if (!GetDevice() || !shaderCode || std::strlen(shaderCode) == 0)
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompile(shaderCode, std::strlen(shaderCode), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, shaderData.ReleaseAndGetAddressOf(), &errorBlob);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = GetDevice()->CreatePixelShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, pixelShader);
			if (FAILED(hr)) {
				logger::error("Failed to compile shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadPixelShaderFile(const std::wstring& shaderFile, ID3D11PixelShader** pixelShader, Blob& shaderData)
		{
			if (!GetDevice() || shaderFile.empty())
				return false;
			Blob errorBlob;
			HRESULT hr = D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, shaderData.ReleaseAndGetAddressOf(), nullptr);
			if (FAILED(hr))
			{
				logger::error("Failed to compile shader {} : {}", hr, errorBlob ? errorBlob->GetBufferPointer() : "Unknown Error");
				return false;
			}
			hr = GetDevice()->CreatePixelShader(shaderData->GetBufferPointer(), shaderData->GetBufferSize(), nullptr, pixelShader);
			if (FAILED(hr))
			{
				logger::error("Failed to create shader {}", hr);
				return false;
			}
			return true;
		}
		bool ShaderManager::LoadCompiledPixelShader(const std::wstring& shaderFile, ID3D11PixelShader** pixelShader)
		{
			if (!GetDevice() || shaderFile.empty())
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
			HRESULT hr = GetDevice()->CreatePixelShader(shaderData.data(), fileSize, nullptr, pixelShader);
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

		ShaderManager::ComputeShader ShaderManager::CreateComputeShader(std::string shaderName)
		{
			std::string shaderName_ = lowLetter(shaderName);
			if (auto found = ComputeShaders.find(shaderName_); found != ComputeShaders.end())
				return found->second;
			if (Failed.find(shaderName_) != Failed.end())
				return nullptr;
			ComputeShader newShader = nullptr;
			Blob csBlob = nullptr;
			if (LoadComputeShaderFile(GetShaderFilePath(shaderName, false, ShaderType::Compute), &newShader, csBlob))
			{
				ComputeShaders[shaderName_] = newShader;
				logger::info("Compile shader done : {}", shaderName);
				SaveCompiledShaderFile(csBlob, GetShaderFilePath(shaderName, true, ShaderType::Compute));
				CSBlobs[shaderName_] = csBlob;
				return newShader;
			}
			logger::warn("Failed to compile shader for {}. so try to load latest compiled shader...", shaderName);
			if (LoadCompiledComputeShader(GetShaderFilePath(shaderName, true, ShaderType::Compute), &newShader))
			{
				ComputeShaders[shaderName_] = newShader;
				logger::info("Loaded latest compiled shader : {}", shaderName);
				D3DReadFileToBlob(GetShaderFilePath(shaderName, true, ShaderType::Compute).c_str(), &csBlob);
				CSBlobs[shaderName_] = csBlob;
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
			Failed.emplace(shaderName);
			return nullptr;
		}
		ShaderManager::VertexShader ShaderManager::CreateVertexShader(std::string shaderName)
		{
			std::string shaderName_ = lowLetter(shaderName);
			if (auto found = VertexShaders.find(shaderName_); found != VertexShaders.end())
				return found->second;
			if (Failed.find(shaderName_) != Failed.end())
				return nullptr;
			VertexShader newShader = nullptr;
			Blob vsBlob = nullptr;
			if (LoadVertexShaderFile(GetShaderFilePath(shaderName, false, ShaderType::Vertex), &newShader, vsBlob))
			{
				VertexShaders[shaderName_] = newShader;
				logger::info("Compile shader done : {}", shaderName);
				SaveCompiledShaderFile(vsBlob, GetShaderFilePath(shaderName, true, ShaderType::Vertex));
				VSBlobs[shaderName_] = vsBlob;
				return newShader;
			}
			logger::warn("Failed to compile shader for {}. so try to load latest compiled shader...", shaderName);
			if (LoadCompiledVertexShader(GetShaderFilePath(shaderName, true, ShaderType::Vertex), &newShader))
			{
				VertexShaders[shaderName_] = newShader;
				logger::info("Loaded latest compiled shader : {}", shaderName);
				D3DReadFileToBlob(GetShaderFilePath(shaderName, true, ShaderType::Vertex).c_str(), &vsBlob);
				VSBlobs[shaderName_] = vsBlob;
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
			Failed.emplace(shaderName);
			return nullptr;
		}
		ShaderManager::PixelShader ShaderManager::CreatePixelShader(std::string shaderName)
		{
			std::string shaderName_ = lowLetter(shaderName);
			if (auto found = PixelShaders.find(shaderName_); found != PixelShaders.end())
				return found->second;
			if (Failed.find(shaderName_) != Failed.end())
				return nullptr;
			PixelShader newShader = nullptr;
			Blob psBlob = nullptr;
			if (LoadPixelShaderFile(GetShaderFilePath(shaderName, false, ShaderType::Pixel), &newShader, psBlob))
			{
				PixelShaders[shaderName_] = newShader;
				logger::info("Compile shader done : {}", shaderName);
				SaveCompiledShaderFile(psBlob, GetShaderFilePath(shaderName, true, ShaderType::Pixel));
				PSBlobs[shaderName_] = psBlob;
				return newShader;
			}
			logger::warn("Failed to compile shader for {}. so try to load latest compiled shader...", shaderName);
			if (LoadCompiledPixelShader(GetShaderFilePath(shaderName, true, ShaderType::Pixel), &newShader))
			{
				PixelShaders[shaderName_] = newShader;
				logger::info("Loaded latest compiled shader : {}", shaderName);
				D3DReadFileToBlob(GetShaderFilePath(shaderName, true, ShaderType::Pixel).c_str(), &psBlob);
				PSBlobs[shaderName_] = psBlob;
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
			Failed.emplace(shaderName);
			return nullptr;
		}

		ShaderManager::ComputeShader ShaderManager::GetComputeShader(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = ComputeShaders.find(shaderName); found != ComputeShaders.end())
				return found->second;
			return CreateComputeShader(shaderName);
		}
		ShaderManager::VertexShader ShaderManager::GetVertexShader(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = VertexShaders.find(shaderName); found != VertexShaders.end())
				return found->second;
			return CreateVertexShader(shaderName);
		}
		ShaderManager::PixelShader ShaderManager::GetPixelShader(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = PixelShaders.find(shaderName); found != PixelShaders.end())
				return found->second;
			return CreatePixelShader(shaderName);
		}

		ShaderManager::Blob ShaderManager::GetComputeShaderBlob(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = CSBlobs.find(shaderName); found != CSBlobs.end())
				return found->second;
			return nullptr;
		}
		ShaderManager::Blob ShaderManager::GetVertexShaderBlob(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = VSBlobs.find(shaderName); found != VSBlobs.end())
				return found->second;
			return nullptr;
		}
		ShaderManager::Blob ShaderManager::GetPixelShaderBlob(std::string shaderName)
		{
			shaderName = lowLetter(shaderName);
			if (auto found = PSBlobs.find(shaderName); found != PSBlobs.end())
				return found->second;
			return nullptr;
		}

		ID3D11DeviceContext* ShaderManager::GetContext()
		{
			if (!context)
				context = reinterpret_cast<ID3D11DeviceContext*>(RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().context);
			return context;
		}
		ID3D11Device* ShaderManager::GetDevice()
		{
			if (!device)
			{
				if (GetContext())
				{
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
					GetContext()->GetDevice(&device);
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				}
			}
			return device;
		}

		RE::NiPointer<RE::NiSourceTexture> TextureLoadManager::GetNiSourceTexture(std::string filePath, std::string name)
		{
			D3D11_TEXTURE2D_DESC texDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
			GetTexture2D(filePath, texDesc, srvDesc, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, texture2d);
			if (!texture2d)
			{
				logger::error("Failed to get texture 2d for NiTexture ({})", filePath);
				return nullptr;
			}
			texture2d->GetDesc(&texDesc);
			srvDesc.Format = texDesc.Format;
			srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;
			HRESULT hr = ShaderManager::GetSingleton().GetDevice()->CreateShaderResourceView(texture2d.Get(), &srvDesc, &textureSRV);
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
				output->rendererTexture->texture = texture2d.Get();
				output->rendererTexture->texture->AddRef();
				output->rendererTexture->resourceView = textureSRV.Get();
				output->rendererTexture->resourceView->AddRef();
				oldTexture->Release();
				oldResource->Release();
			}
			else if (result == 1)
			{
				RE::BSGraphics::Texture* newRendererTexture = new RE::BSGraphics::Texture();
				newRendererTexture->texture = texture2d.Get();
				newRendererTexture->texture->AddRef();
				newRendererTexture->resourceView = textureSRV.Get();
				newRendererTexture->resourceView->AddRef();
				output->rendererTexture = newRendererTexture;
			}
			SetOrgTexturePath(name, filePath);
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

		void TextureLoadManager::CreateNiTexture(std::string name, std::string texturePath, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSRV, RE::NiPointer<RE::NiSourceTexture>& output, bool& texCreated)
		{
			RE::NiPointer<RE::NiSourceTexture> newTexture;
			auto result = TextureLoadManager::CreateSourceTexture(name, newTexture);
			if (result == -1 || !newTexture)
			{
				logger::critical("Failed to create NiTexture");
				return;
			}
			else if (result == 0)
			{
				auto oldTexture = newTexture->rendererTexture->texture;
				auto oldResource = newTexture->rendererTexture->resourceView;
				newTexture->rendererTexture->texture = dstTex.Get();
				newTexture->rendererTexture->texture->AddRef();
				newTexture->rendererTexture->resourceView = dstSRV.Get();
				newTexture->rendererTexture->resourceView->AddRef();
				oldTexture->Release();
				oldResource->Release();
				texCreated = false;
			}
			else if (result == 1)
			{
				RE::BSGraphics::Texture* newRendererTexture = new RE::BSGraphics::Texture();
				newRendererTexture->texture = dstTex.Get();
				newRendererTexture->texture->AddRef();
				newRendererTexture->resourceView = dstSRV.Get();
				newRendererTexture->resourceView->AddRef();
				newTexture->rendererTexture = newRendererTexture;
				texCreated = true;
			}
			output = newTexture;
			SetOrgTexturePath(name, texturePath);
			return;
		}

		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			filePath = stringRemoveStarts(filePath, "Data\\");
			if (!stringStartsWith(filePath, "textures"))
				filePath = "Textures\\" + filePath;
			filePath = FixPath(filePath);

			if (auto found = textures.find(filePath); found != textures.end() &&
				(found->second.metaData.newFormat == newFormat && found->second.metaData.newWidth == newWidth && found->second.metaData.newHeight == newHeight))
			{
				textureDesc = found->second.texDesc;
				srvDesc = found->second.srvDesc;
				return CopyTexture(found->second.texture, output);
			}

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
			textureData data;
			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			sourceTexture->rendererTexture->resourceView->GetDesc(&data.srvDesc);
			sourceTexture->rendererTexture->resourceView->GetResource(&resource);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
			HRESULT hr = resource.As(&texture2D);
			if (FAILED(hr))
			{
				logger::error("Failed to load texture resource ({})", hr);
				return false;
			}
			texture2D->GetDesc(&data.texDesc);
			data.metaData.newFormat = newFormat;
			data.metaData.newWidth = newWidth;
			data.metaData.newHeight = newHeight;
			ConvertTexture(texture2D, data.metaData.newFormat, data.metaData.newWidth, data.metaData.newHeight, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, data.texture);
			if (!data.texture)
			{
				logger::error("Failed to convert texture : {}", filePath);
				return false;
			}
			textures[filePath] =  data;

			textureDesc = data.texDesc;
			srvDesc = data.srvDesc;
			return CopyTexture(data.texture, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			return GetTexture2D(filePath, textureDesc, srvDesc, newFormat, 0, 0, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_TEXTURE2D_DESC tmpTexDesc;
			return GetTexture2D(filePath, tmpTexDesc, srvDesc, newFormat, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC tmpSRVDesc;
			return GetTexture2D(filePath, textureDesc, tmpSRVDesc, newFormat, output);
		}
		bool TextureLoadManager::GetTexture2D(std::string filePath, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			D3D11_TEXTURE2D_DESC tmpTexDesc;
			D3D11_SHADER_RESOURCE_VIEW_DESC tmpSRVDesc;
			return GetTexture2D(filePath, tmpTexDesc, tmpSRVDesc, newFormat, output);
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

			auto found = textures.find(filePath);
			if (found == textures.end())
				return true;
			RE::NiPointer<RE::NiSourceTexture> sourceTexture;
			LoadTexture(filePath.c_str(), 1, sourceTexture, false);
			if (!sourceTexture || !sourceTexture->rendererTexture || !sourceTexture->rendererTexture->resourceView)
			{
				logger::error("Failed to load texture file : {}", filePath);
				return false;
			}
			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			sourceTexture->rendererTexture->resourceView->GetDesc(&found->second.srvDesc);
			sourceTexture->rendererTexture->resourceView->GetResource(&resource);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D;
			HRESULT hr = resource.As(&texture2D);
			if (FAILED(hr))
			{
				logger::error("Failed to load texture resource ({})", hr);
				return false;
			}
			texture2D->GetDesc(&found->second.texDesc);
			ConvertTexture(texture2D, found->second.metaData.newFormat, found->second.metaData.newWidth, found->second.metaData.newHeight, DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT, found->second.texture);
			if (!found->second.texture)
			{
				logger::error("Failed to convert texture : {}", filePath);
				return false;
			}
			logger::info("Update Texture Done : {}", filePath);
			return true;
		}
		bool TextureLoadManager::UpdateTexturesAll()
		{
			bool result = true;
			for (auto& texture : textures)
			{
				result = UpdateTexture(texture.first) ? result : false;
			}
			return result;
		}
		std::string TextureLoadManager::GetOrgTexturePath(std::string name)
		{
			auto found = textureOrgPath.find(name);
			if (found == textureOrgPath.end())
				return "";
			return found->second;
		}
		void TextureLoadManager::SetOrgTexturePath(std::string name, std::string texturePath)
		{
			if (texturePath.empty())
				texturePath = "None";
			textureOrgPath[name] = texturePath;
		}
		bool TextureLoadManager::UpdateNiTexture(std::string filePath)
		{
			if (!IsExistFile(filePath, ExistType::textures, true))
				return true;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			if (!GetTextureFromFile(filePath, texture, srv))
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

			auto oldTexture = sourceTexture->rendererTexture->texture;
			auto oldResource = sourceTexture->rendererTexture->resourceView;
			sourceTexture->rendererTexture->texture = texture.Get();
			sourceTexture->rendererTexture->texture->AddRef();
			sourceTexture->rendererTexture->resourceView = srv.Get();
			sourceTexture->rendererTexture->resourceView->AddRef();
			oldTexture->Release();
			oldResource->Release();
			return true;
		}
		bool TextureLoadManager::CopyTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture)
		{
			if (!srcTexture)
				return false;

			ID3D11DeviceContext* context = ShaderManager::GetSingleton().GetContext();
			ID3D11Device* device = ShaderManager::GetSingleton().GetDevice();

			D3D11_TEXTURE2D_DESC textureDesc;
			srcTexture->GetDesc(&textureDesc);

			HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, dstTexture.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to copy texture ({})", hr);
				return false;
			}
			ShaderManager::GetSingleton().ShaderContextLock();
			context->CopyResource(dstTexture.Get(), srcTexture.Get());
			ShaderManager::GetSingleton().ShaderContextUnlock();
			return true;
		}
		bool TextureLoadManager::ConvertTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, DirectX::TEX_FILTER_FLAGS resizeFilter, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output)
		{
			if (!texture)
				return false;

			ID3D11DeviceContext* context = ShaderManager::GetSingleton().GetContext();
			ID3D11Device* device = ShaderManager::GetSingleton().GetDevice();

			D3D11_TEXTURE2D_DESC textureDesc;
			texture->GetDesc(&textureDesc);

			// decoding texture
			DirectX::ScratchImage image;
			ShaderManager::GetSingleton().ShaderContextLock();
			HRESULT hr = DirectX::CaptureTexture(device, context, texture.Get(), image);
			ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to decoding texture ({})", hr);
				return false;
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> tmpTexture;
			if (newFormat != DXGI_FORMAT_UNKNOWN && textureDesc.Format != newFormat)
			{
				// convert format
				DirectX::ScratchImage convertedImage;
				hr = DirectX::Decompress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, convertedImage);
				if (FAILED(hr))
				{
					hr = DirectX::Convert(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, convertedImage);
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
					ConvertD3D11(resize, tmpTexture);
				}
				else
				{
					ConvertD3D11(convertedImage, tmpTexture);
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
					ConvertD3D11(resize, tmpTexture);
				}
				else
				{
					ConvertD3D11(image, tmpTexture);
				}
			}
			if (!tmpTexture)
			{
				logger::error("Failed to convert texture to d3d11 resource");
				return false;
			}
			
			output.Reset();
			hr = tmpTexture.As(&output);
			if (FAILED(hr))
			{
				logger::error("Failed to convert texture to d3d11 resource ({})", hr);
				return false;
			}
			return true;
		}
		bool TextureLoadManager::ConvertD3D11(DirectX::ScratchImage& image, Microsoft::WRL::ComPtr<ID3D11Resource>& output)
		{
			// convert texture to d3d11 texture
			HRESULT hr = DirectX::CreateTexture(ShaderManager::GetSingleton().GetDevice(), image.GetImages(), image.GetImageCount(), image.GetMetadata(), output.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to convert texture to d3d11 ({})", hr);
				return false;
			}
			return true;
		}
		bool TextureLoadManager::CompressTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture)
		{
			auto formatType = IsCompressFormat(newFormat);
			if (formatType < 0)
				return false;

			ID3D11DeviceContext* context = ShaderManager::GetSingleton().GetContext();
			ID3D11Device* device = ShaderManager::GetSingleton().GetDevice();

			// decoding texture
			DirectX::ScratchImage image;
			ShaderManager::GetSingleton().ShaderContextLock();
			HRESULT hr = DirectX::CaptureTexture(device, context, srcTexture.Get(), image);
			ShaderManager::GetSingleton().ShaderContextUnlock();
			if (FAILED(hr))
			{
				logger::error("Failed to decoding texture ({})", hr);
				return false;
			}

			DirectX::ScratchImage compressedImage;
			if (formatType == 1) //CPU format
			{
				hr = DirectX::Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, DirectX::TEX_COMPRESS_PARALLEL, DirectX::TEX_THRESHOLD_DEFAULT, compressedImage);
				if (FAILED(hr))
				{
					logger::error("Failed to compress texture to {} ({})", magic_enum::enum_name(newFormat).data(), hr);
					return false;
				}
			}
			else if (formatType == 2) //GPU format
			{
				hr = DirectX::Compress(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), newFormat, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressedImage);
				if (FAILED(hr))
				{
					logger::error("Failed to compress texture to {} ({})", magic_enum::enum_name(newFormat).data(), hr);
					return false;
				}
			}

			Microsoft::WRL::ComPtr<ID3D11Resource> tmpTexture;
			ConvertD3D11(compressedImage, tmpTexture);
			if (!tmpTexture)
			{
				logger::error("Failed to convert texture to d3d11 resource");
				return false;
			}

			dstTexture.Reset();
			hr = tmpTexture.As(&dstTexture);
			if (FAILED(hr))
			{
				logger::error("Failed to convert texture to d3d11 resource ({})", hr);
				return false;
			}
			return true;
		}
		bool TextureLoadManager::GetTextureFromFile(std::string filePath, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
		{
			if (!stringStartsWith(filePath, "Data"))
				filePath = "Data\\" + filePath; 
			if (!stringEndsWith(filePath, ".dds"))
				return false;
			DirectX::ScratchImage image;
			HRESULT hr = LoadFromDDSFile(string2wstring(filePath).c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_NONE, nullptr, image);
			if (FAILED(hr)) {
				logger::error("Failed to get texture from {} file", filePath);
				return false;
			}
			Microsoft::WRL::ComPtr<ID3D11Resource> resource;
			if (!ConvertD3D11(image, resource))
			{
				logger::error("Failed to get texture from {} file", filePath);
				return false;
			}
			texture.Reset();
			hr = resource.As(&texture);
			if (FAILED(hr))
			{
				logger::error("Failed to convert texture to d3d11 resource ({})", hr);
				return false;
			}

			D3D11_TEXTURE2D_DESC textureDesc;
			texture->GetDesc(&textureDesc);
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = textureDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;

			ID3D11Device* device = ShaderManager::GetSingleton().GetDevice();
			hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				logger::error("Failed to create ShaderResourceView : {} / ({})", filePath, hr);
				return false;
			}
			return true;
		}
	}

	//https://www.codespeedy.com/hsv-to-rgb-in-cpp/
	RGBA HSVA::HSVAtoRGBA(HSVA hsva)
	{
		RGBA color = RGBA();
		if (hsva.hue > 360 || hsva.hue < 0 || hsva.saturation > 100 || hsva.saturation < 0 || hsva.value > 100 || hsva.value < 0)
		{
			color.r = 1;
			color.g = 1;
			color.b = 1;
			color.a = 1;
			return color;
		}
		float s = hsva.saturation / 100;
		float v = hsva.value / 100;
		float C = s * v;
		float X = C * (1 - abs(fmod(hsva.hue / 60.0, 2) - 1));
		float m = v - C;
		float r, g, b;
		if (hsva.hue >= 0 && hsva.hue < 60) {
			r = C, g = X, b = 0;
		}
		else if (hsva.hue >= 60 && hsva.hue < 120) {
			r = X, g = C, b = 0;
		}
		else if (hsva.hue >= 120 && hsva.hue < 180) {
			r = 0, g = C, b = X;
		}
		else if (hsva.hue >= 180 && hsva.hue < 240) {
			r = 0, g = X, b = C;
		}
		else if (hsva.hue >= 240 && hsva.hue < 300) {
			r = X, g = 0, b = C;
		}
		else {
			r = C, g = 0, b = X;
		}

		color.r = (r + m);
		color.g = (g + m);
		color.b = (b + m);
		color.a = hsva.alpha * 0.01f;
		return color;
	}
	HSVA HSVA::RGBAtoHSVA(RGBA rgba)
	{
		HSVA hsv = HSVA();
		float r = rgba.r;
		float g = rgba.g;
		float b = rgba.b;
		float h = 0.0f;
		float s = 0.0f;
		float v = 0.0f;
		float cmax = std::max(std::max(r, g), b);
		float cmin = std::min(std::min(r, g), b);
		float delta = cmax - cmin;

		if (delta > 0) {
			if (cmax == r) {
				h = 60 * (fmod(((g - b) / delta), 6));
			}
			else if (cmax == g) {
				h = 60 * (((b - r) / delta) + 2);
			}
			else if (cmax == b) {
				h = 60 * (((r - g) / delta) + 4);
			}
			if (cmax > 0) {
				s = delta / cmax * 100;
			}
			else {
				s = 0;
			}
			v = cmax * 100;
		}
		else {
			h = 0;
			s = 0;
			v = cmax * 100;
		}
		if (h < 0) {
			h = 360 + h;
		}

		hsv.hue = h;
		hsv.saturation = s;
		hsv.value = v;
		hsv.alpha = rgba.a * 100.0f;
		return hsv;
	}
}
