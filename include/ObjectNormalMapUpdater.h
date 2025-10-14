#pragma once

namespace Mus {
	class ObjectNormalMapUpdater :
		public IEventListener<FrameEvent>,
		public IEventListener<PlayerCellChangeEvent> {
	public:
		ObjectNormalMapUpdater() {};
		~ObjectNormalMapUpdater() {};

		[[nodiscard]] static ObjectNormalMapUpdater& GetSingleton() {
			static ObjectNormalMapUpdater instance;
			return instance;
		}

		void Init();
		bool CreateGeometryResourceData(RE::FormID a_actorID, GeometryDataPtr a_data);

		void ClearGeometryResourceData();

		inline bool IsBeforeTaskReleased() {
			if (Config::GetSingleton().GetVRAMSaveMode())
			{
				ResourceDataMapLock.lock_shared();
				bool isEmpty = ResourceDataMap.empty();
				ResourceDataMapLock.unlock_shared();
				return isEmpty;
			}
			return true;
		};

		struct NormalMapResult {
			bool existResource = false;
			bool diskCache = false;
			std::uint64_t hash = 0;
			bSlot slot;
			RE::BSGeometry* geometry = nullptr; //for ptr compare only 
			std::string geoName = "";
			std::string texturePath = "";
			std::string textureName = "";
			std::uint32_t vertexCount = 0;
			TextureResourcePtr texture;
		};
		typedef concurrency::concurrent_vector<NormalMapResult> UpdateResult;
		UpdateResult UpdateObjectNormalMap(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet);
		UpdateResult UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet);

	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const PlayerCellChangeEvent& e) override;

	private:
		inline ID3D11Device* GetDevice() const { 
			if (Config::GetSingleton().GetGPUDeviceIndex() != 0)
			{
				return Shader::ShaderManager::GetSingleton().IsValidSecondGPU() ?
					Shader::ShaderManager::GetSingleton().GetSecondDevice() : Shader::ShaderManager::GetSingleton().GetDevice();
			}
			return Shader::ShaderManager::GetSingleton().GetDevice();
		};
		inline ID3D11DeviceContext* GetContext() const { 
			if (Config::GetSingleton().GetGPUDeviceIndex() != 0)
			{
				return Shader::ShaderManager::GetSingleton().IsValidSecondGPU() ?
					Shader::ShaderManager::GetSingleton().GetSecondContext() : Shader::ShaderManager::GetSingleton().GetContext();
			}
			return Shader::ShaderManager::GetSingleton().GetContext();
		};

		bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texOutput, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOutput);
		bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texOutput, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOutput);
		bool LoadTextureCPU(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);
		bool LoadTextureCPU(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);

		struct TextureResourceData {
			std::string textureName;
			std::clock_t time = -1;

			Microsoft::WRL::ComPtr<ID3D11Query> query = nullptr;
			bool GetQuery(ID3D11Device* device, ID3D11DeviceContext* context) {
				if (!device || !context)
					return false;

				isQuaryUseSecondGPU = Shader::ShaderManager::GetSingleton().IsValidSecondGPU();
				D3D11_QUERY_DESC queryDesc = {};
				queryDesc.Query = D3D11_QUERY_EVENT;
				queryDesc.MiscFlags = 0;

				HRESULT hr = device->CreateQuery(&queryDesc, &query);
				if (FAILED(hr)) {
					query = nullptr;
					return false;
				}
				QuaryShaderLock();
				context->End(query.Get());
				QuaryShaderUnlock();
				queryContext = context;
				return true;
			}
			bool IsQueryDone() {
				if (!queryContext || !query)
					return true; 
				QuaryShaderLock();
				HRESULT hr = queryContext->GetData(query.Get(), nullptr, 0, 0);
				QuaryShaderUnlock();
				if (FAILED(hr))
					return true;
				return hr == S_OK;
			}

			ID3D11DeviceContext* queryContext = nullptr;
			bool isQuaryUseSecondGPU = false;
			void QuaryShaderLock() const {
				if (isQuaryUseSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
			};
			void QuaryShaderUnlock() const {
				if (isQuaryUseSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			};

			std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> detailTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> detailShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> overlayTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> overlayShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> maskTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> dstWriteTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstWriteTextureUAV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstShaderResourceView = nullptr;

			struct BleedTextureData {
				std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D = nullptr;
				std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs;
			};
			BleedTextureData bleedTextureData;

			struct CopySecondToMainData {
				Microsoft::WRL::ComPtr<ID3D11Texture2D> copyTexture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcOldTexture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcOldShaderResourceView = nullptr;
			};
			CopySecondToMainData copySecondToMainData;

			struct MergeTextureData {
				std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTexture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav = nullptr;
			};
			MergeTextureData mergeTextureData;

			struct TextureCompressData {
				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcOldTexture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcOldShaderResourceView = nullptr;
			};
			TextureCompressData textureCompressData;
		};
		typedef std::shared_ptr<TextureResourceData> TextureResourceDataPtr;

		bool IsDetailNormalMap(const std::string& a_normalMapPath);

		DirectX::XMVECTOR SlerpVector(const DirectX::XMVECTOR& a, const DirectX::XMVECTOR& b, const float& t);
		bool ComputeBarycentric(const float& px, const float& py, const DirectX::XMINT2& a, const DirectX::XMINT2& b, const DirectX::XMINT2& c, DirectX::XMFLOAT3& out);
		bool CreateStructuredBuffer(ID3D11Device* device, const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);

		bool BleedTexture(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries);
		bool BleedTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11Texture2D* texInOut);
		bool BleedTextureGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* srvInOut, ID3D11Texture2D* texInOut);

		bool MergeTexture(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries);
		bool MergeTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11Texture2D* dstTex, ID3D11Texture2D* srcTex);
		bool MergeTextureGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* dstSrv, ID3D11Texture2D* dstTex,ID3D11ShaderResourceView* srcSrv, ID3D11Texture2D* srcTex);

		bool GenerateMipMap(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries);

		bool CopySubresourceRegion(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* dstTexture, ID3D11Texture2D* srcTexture, UINT dstMipMapLevel, UINT srcMipMapLevel);
		
		bool CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries);
		bool CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut);
	
		void CopyResourceToMain(ID3D11Device* device, ID3D11DeviceContext* context, concurrency::concurrent_vector<TextureResourceDataPtr>& resourceDatas, UpdateResult& results, std::unordered_set<RE::BSGeometry*>& mergedTextureGeometries);
		bool CopyResourceSecondToMain(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut);
		
		void GPUPerformanceLog(ID3D11Device* device, ID3D11DeviceContext* context, std::string funcStr, bool isEnd, bool isAverage = true, std::uint32_t args = 0);

		class WaitForGPU {
		public:
			WaitForGPU() = delete;
			WaitForGPU(ID3D11Device* a_device, ID3D11DeviceContext* a_context, bool a_secondGPUNoWait = true);
			~WaitForGPU() {};

			void Wait();

		private:
			ID3D11Device* device = nullptr;
			ID3D11DeviceContext* context = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Query> query = nullptr;
			bool isSecondGPU = false;
			inline void ShaderLock() const {
				if (isSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextLock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextLock();
			};
			inline void ShaderUnlock() const {
				if (isSecondGPU)
					Shader::ShaderManager::GetSingleton().ShaderSecondContextUnlock();
				else
					Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
			};
			bool secondGPUNoWait = true;
		};

		void WaitForFreeVram();

		const std::string_view BleedTextureShaderName = "BleedTexture";
		const std::string_view UpdateNormalMapShaderName = "UpdateNormalMap";
		const std::string_view MergeTextureShaderName = "MergeTexture";
		const std::string_view TexturePostProcessingShaderName = "TexturePostProcessing";

		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState = nullptr;

		struct GeometryResourceData {
			Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Buffer> uvBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Buffer> normalBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Buffer> tangentBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tangentSRV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Buffer> bitangentBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bitangentSRV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Buffer> indicesBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> indicesSRV = nullptr;
		};
		typedef std::shared_ptr<GeometryResourceData> GeometryResourceDataPtr;
		concurrency::concurrent_unordered_map<RE::FormID, GeometryResourceDataPtr> GeometryResourceDataMap;
		GeometryResourceDataPtr GetGeometryResourceData(RE::FormID a_actorID);

		std::shared_mutex ResourceDataMapLock;
		concurrency::concurrent_vector<TextureResourceDataPtr> ResourceDataMap;

		std::uint64_t GetHash(UpdateTextureSet updateSet, std::uint64_t geoHash);
	};
}