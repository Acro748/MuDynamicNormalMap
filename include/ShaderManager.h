#pragma once

namespace Mus {
	namespace Shader {
		class ShaderManager {
		public:
			[[nodiscard]] static ShaderManager& GetSingleton() {
				static ShaderManager instance;
				return instance;
			}

			ShaderManager() {};
			~ShaderManager() {};

			typedef Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShader;
			typedef Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
			typedef Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
			typedef Microsoft::WRL::ComPtr<ID3DBlob> Blob;
			typedef Microsoft::WRL::ComPtr<ID3D11Device> Device;

			ComputeShader GetComputeShader(std::string shaderName);
			VertexShader GetVertexShader(std::string shaderName);
			PixelShader GetPixelShader(std::string shaderName);

			Blob GetComputeShaderBlob(std::string shaderName);
			Blob GetVertexShaderBlob(std::string shaderName);
			Blob GetPixelShaderBlob(std::string shaderName);

			ID3D11DeviceContext* GetContext();
			ID3D11Device* GetDevice();
			inline void ShaderContextLock() { EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&RE::BSGraphics::Renderer::GetSingleton()->GetLock())); };
			inline void ShaderContextUnlock() { LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&RE::BSGraphics::Renderer::GetSingleton()->GetLock())); };

			void Flush();

			class ShaderLockGuard {
			public:
				ShaderLockGuard() { EnterCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&RE::BSGraphics::Renderer::GetSingleton()->GetLock())); };
				~ShaderLockGuard() { LeaveCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(&RE::BSGraphics::Renderer::GetSingleton()->GetLock())); };

				ShaderLockGuard(const ShaderLockGuard&) = delete;
				ShaderLockGuard& operator=(const ShaderLockGuard&) = delete;
			};

			bool IsFailedShader(std::string shaderName);

			void ResetShader() { ComputeShaders.clear(); VertexShaders.clear(); PixelShaders.clear(); Failed.clear(); };
		private:
			bool SaveCompiledShaderFile(Microsoft::WRL::ComPtr<ID3DBlob>& compiledShader, const std::wstring& shaderFile);

			bool LoadComputeShader(const char* shaderCode, ID3D11ComputeShader** computeShader, Microsoft::WRL::ComPtr<ID3DBlob>& shaderData);
			bool LoadComputeShaderFile(const std::wstring& shaderFile, ID3D11ComputeShader** computeShader, Microsoft::WRL::ComPtr<ID3DBlob>& shaderData);
			bool LoadCompiledComputeShader(const std::wstring& shaderFile, ID3D11ComputeShader** computeShader);

			bool LoadVertexShader(const char* shaderCode, ID3D11VertexShader** vertexShader, Microsoft::WRL::ComPtr<ID3DBlob>& shaderData);
			bool LoadVertexShaderFile(const std::wstring& shaderFile, ID3D11VertexShader** vertexShader, Microsoft::WRL::ComPtr<ID3DBlob>& shaderData);
			bool LoadCompiledVertexShader(const std::wstring& shaderFile, ID3D11VertexShader** vertexShader);

			bool LoadPixelShader(const char* shaderCode, ID3D11PixelShader** pixelShader, Microsoft::WRL::ComPtr<ID3DBlob>& shaderData);
			bool LoadPixelShaderFile(const std::wstring& shaderFile, ID3D11PixelShader** pixelShader, Microsoft::WRL::ComPtr<ID3DBlob>& shaderData);
			bool LoadCompiledPixelShader(const std::wstring& shaderFile, ID3D11PixelShader** pixelShader);

			enum ShaderType {
				Compute,
				Vertex,
				Pixel
			};
			std::wstring GetShaderFilePath(std::string shaderName, bool compiled, ShaderType shaderType);
			ComputeShader CreateComputeShader(std::string shaderName);
			VertexShader CreateVertexShader(std::string shaderName);
			PixelShader CreatePixelShader(std::string shaderName);

			ID3D11DeviceContext* context;
			ID3D11Device* device;

			concurrency::concurrent_unordered_map<std::string, ComputeShader> ComputeShaders;
			concurrency::concurrent_unordered_map<std::string, VertexShader> VertexShaders;
			concurrency::concurrent_unordered_map<std::string, PixelShader> PixelShaders;

			concurrency::concurrent_unordered_map<std::string, Blob> CSBlobs;
			concurrency::concurrent_unordered_map<std::string, Blob> VSBlobs;
			concurrency::concurrent_unordered_map<std::string, Blob> PSBlobs;

			std::unordered_set<std::string> Failed;
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
			bool GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, D3D11_TEXTURE2D_DESC& textureDesc, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool GetTexture2D(std::string filePath, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
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

			bool ConvertTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, DXGI_FORMAT newFormat, UINT newWidth, UINT newHeight, DirectX::TEX_FILTER_FLAGS resizeFilter, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
			bool CopyTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture);
			bool CompressTexture(Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, DXGI_FORMAT newFormat, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture);
			bool GetTextureFromFile(std::string filePath, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv);

			std::int8_t IsCompressFormat(DXGI_FORMAT format); // -1 non compress, 1 cpu compress, 2 gpu compress

			std::int8_t CreateNiTexture(std::string name, Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstSRV, RE::NiPointer<RE::NiSourceTexture>& output);
			void ReleaseNiTexture(std::string name);
		private:
			bool ConvertD3D11(DirectX::ScratchImage& image, Microsoft::WRL::ComPtr<ID3D11Resource>& output);
			bool UpdateNiTexture(std::string filePath);

			struct textureData {
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
				D3D11_TEXTURE2D_DESC texDesc;
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
				struct MetaData {
					DXGI_FORMAT newFormat;
					UINT newWidth;
					UINT newHeight;
				};
				MetaData metaData;
			};
			concurrency::concurrent_unordered_map<std::string, RE::NiPointer<RE::NiSourceTexture>> niTextures;
		};
	}
	struct HSVA {
		float hue = 0; // 0~360
		float saturation = 0; // 0~100
		float value = 100; // 0~100
		float alpha = 100; // 0~100

		static RGBA HSVAtoRGBA(HSVA hsva);
		static HSVA RGBAtoHSVA(RGBA rgba);
	};
}