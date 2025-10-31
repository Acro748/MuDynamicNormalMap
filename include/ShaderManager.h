#pragma once

namespace Mus {
	namespace Shader {
		class ShaderManager {
		public:
			[[nodiscard]] static ShaderManager& GetSingleton() {
				static ShaderManager instance;
				return instance;
			}

			ShaderManager() { InitializeCriticalSectionAndSpinCount(&subContextLock, 4000); };
			~ShaderManager() {};

			typedef Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShader;
			typedef Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
			typedef Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
			typedef Microsoft::WRL::ComPtr<ID3DBlob> Blob;
			typedef Microsoft::WRL::ComPtr<ID3D11Device> Device;

			ComputeShader GetComputeShader(ID3D11Device* device, std::string shaderName);
			VertexShader GetVertexShader(ID3D11Device* device, std::string shaderName);
			PixelShader GetPixelShader(ID3D11Device* device, std::string shaderName);

			Blob GetComputeShaderBlob(std::string shaderName);
			Blob GetVertexShaderBlob(std::string shaderName);
			Blob GetPixelShaderBlob(std::string shaderName);

			bool CreateDeviceContextWithSecondGPUImpl();
			bool CreateDeviceContextWithSecondGPU();

			ID3D11DeviceContext* GetContext();
			ID3D11Device* GetDevice();

			inline void ShaderContextLock() { EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&RE::BSGraphics::Renderer::GetSingleton()->GetLock())); };
			inline void ShaderContextUnlock() { LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&RE::BSGraphics::Renderer::GetSingleton()->GetLock())); };

			ID3D11DeviceContext* GetSecondContext();
			ID3D11Device* GetSecondDevice();
			bool IsValidSecondGPU();
			inline void ShaderSecondContextLock() { EnterCriticalSection(&subContextLock); };
			inline void ShaderSecondContextUnlock() { LeaveCriticalSection(&subContextLock); };
			inline void SetSearchSecondGPU(bool isSearch) { isSearchSecondGPU = isSearch; };

			bool IsSecondGPUResource(ID3D11DeviceContext* context) { return context == secondContext_.Get(); };
			bool IsSecondGPUResource(ID3D11Device* device) { return device == secondDevice_.Get(); };

			bool IsFailedShader(std::string shaderName);

			void ResetShader() { computeShaders.clear(); vertexShaders.clear(); pixelShaders.clear(); failedList.clear(); };
		private:
			bool SaveCompiledShaderFile(Blob& compiledShader, const std::wstring& shaderFile);

			bool LoadComputeShader(ID3D11Device* device, const char* shaderCode, ID3D11ComputeShader** computeShader, Blob& shaderData);
			bool LoadComputeShaderFile(ID3D11Device* device, const std::wstring& shaderFile, ID3D11ComputeShader** computeShader, Blob& shaderData);
			bool LoadCompiledComputeShader(ID3D11Device* device, const std::wstring& shaderFile, ID3D11ComputeShader** computeShader);

			bool LoadVertexShader(ID3D11Device* device, const char* shaderCode, ID3D11VertexShader** vertexShader, Blob& shaderData);
			bool LoadVertexShaderFile(ID3D11Device* device, const std::wstring& shaderFile, ID3D11VertexShader** vertexShader, Blob& shaderData);
			bool LoadCompiledVertexShader(ID3D11Device* device, const std::wstring& shaderFile, ID3D11VertexShader** vertexShader);

			bool LoadPixelShader(ID3D11Device* device, const char* shaderCode, ID3D11PixelShader** pixelShader, Blob& shaderData);
			bool LoadPixelShaderFile(ID3D11Device* device, const std::wstring& shaderFile, ID3D11PixelShader** pixelShader, Blob& shaderData);
			bool LoadCompiledPixelShader(ID3D11Device* device, const std::wstring& shaderFile, ID3D11PixelShader** pixelShader);

			enum ShaderType {
				Compute,
				Vertex,
				Pixel
			};
			std::wstring GetShaderFilePath(std::string shaderName, bool compiled, ShaderType shaderType);
			ComputeShader CreateComputeShader(ID3D11Device* device, std::string shaderName);
			VertexShader CreateVertexShader(ID3D11Device* device, std::string shaderName);
			PixelShader CreatePixelShader(ID3D11Device* device, std::string shaderName);

			ID3D11DeviceContext* context_;
			ID3D11Device* device_;

			CRITICAL_SECTION subContextLock;
			Microsoft::WRL::ComPtr<ID3D11DeviceContext> secondContext_;
			Microsoft::WRL::ComPtr<ID3D11Device> secondDevice_;
			bool isSearchSecondGPU = true;

			concurrency::concurrent_unordered_map<std::string, ComputeShader> computeShaders;
			concurrency::concurrent_unordered_map<std::string, VertexShader> vertexShaders;
			concurrency::concurrent_unordered_map<std::string, PixelShader> pixelShaders;

			concurrency::concurrent_unordered_map<std::string, Blob> csBlobs;
			concurrency::concurrent_unordered_map<std::string, Blob> vsBlobs;
			concurrency::concurrent_unordered_map<std::string, Blob> psBlobs;

			std::unordered_set<std::string> failedList;
		};

		class ShaderLocker {
		public:
			ShaderLocker() = delete;
			ShaderLocker(ID3D11DeviceContext* context) : isSecondGPU(Shader::ShaderManager::GetSingleton().IsSecondGPUResource(context)) {};
			inline void Lock() {
				if (isSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
			};
			inline void Unlock() {
				if (isSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			};
			bool IsSecondGPU() const { return isSecondGPU; };
		private:
			const bool isSecondGPU = false;
		};

		class TextureLoadManager {
		public:
			[[nodiscard]] static TextureLoadManager& GetSingleton() {
				static TextureLoadManager instance;
				return instance;
			}

			TextureLoadManager() {};
			~TextureLoadManager() {};

			RE::NiPointer<RE::NiSourceTexture> GetNiSourceTexture(std::string filePath, std::string name);
			bool GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);

			bool GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, std::string filePath, DXGI_FORMAT newFormat, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);

			bool UpdateTexture(std::string filePath);

			static RE::NiTexture* CreateTexture(const RE::BSFixedString& name)
			{
				using func_t = decltype(&TextureLoadManager::CreateTexture);
				REL::VariantID offset(69335, 70717, 0x00CAEF60);
				REL::Relocation<func_t> func{ offset };
				return func(name);
			}
			static std::int8_t CreateSourceTexture(std::string name, RE::NiPointer<RE::NiSourceTexture>& output) // -1 : failed to load/create, 0 : loaded, 1 : created
			{
				if (auto found = GetSingleton().niTextures.find(name); found != GetSingleton().niTextures.end())
				{
					if (found->second)
					{
						output = found->second;
						return 0;
					}
				}
				auto newSourceTexture = RE::NiPointer(static_cast<RE::NiSourceTexture*>(TextureLoadManager::CreateTexture(name.c_str())));
				if (!newSourceTexture)
					return -1;
				nif::setBSFixedString(newSourceTexture->name, name.c_str());
				GetSingleton().niTextures[name] = newSourceTexture;
				output = newSourceTexture;
				return 1;
			}

			static void LoadTexture(const char* path, std::uint8_t unk1, RE::NiPointer<RE::NiSourceTexture>& texture, bool unk2)
			{
				using func_t = decltype(&TextureLoadManager::LoadTexture);
				REL::Relocation<func_t> func{ RELOCATION_ID(98986, 105640) };
				return func(path, unk1, texture, unk2);
			}

			static std::uint32_t InitializeShader(RE::BSLightingShaderProperty* shaderProperty, RE::BSGeometry* geometry) {
				using func_t = decltype(&TextureLoadManager::InitializeShader);
				REL::VariantID offset(99862, 106507, 0x01303AC0);
				REL::Relocation<func_t> func{ offset };
				return func(shaderProperty, geometry);
			}

			static void InvalidateTextures(RE::BSLightingShaderProperty* shaderProperty, std::uint32_t unk1) {
				using func_t = decltype(&TextureLoadManager::InvalidateTextures);
				REL::VariantID offset(99865, 106510, 0x01303EA0);
				REL::Relocation<func_t> func{ offset };
				return func(shaderProperty, unk1);
			}

			bool ConvertTexture(ID3D11Device* device, ID3D11DeviceContext* context, Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, DirectX::TEX_FILTER_FLAGS resizeFilter, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, DXGI_FORMAT newFormat, bool cpuReadable, std::uint8_t quality, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		
			std::int8_t IsCompressFormat(DXGI_FORMAT format); // -1 non compress, 1 cpu compress, 2 gpu compress

			std::int8_t CreateNiTexture(std::string name, Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstSRV, RE::NiPointer<RE::NiSourceTexture>& output);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> GetNiTexture(std::string name);
			void ReleaseNiTexture(std::string name);

			bool PrintTexture(std::string filePath, ID3D11Texture2D* texture);
		private:
			bool ConvertD3D11(ID3D11Device* device, DirectX::ScratchImage& image, bool cpuReadable, Microsoft::WRL::ComPtr<ID3D11Resource>& output);
			bool UpdateNiTexture(std::string filePath);

			concurrency::concurrent_unordered_map<std::string, RE::NiPointer<RE::NiSourceTexture>> niTextures;
		};
	}
}