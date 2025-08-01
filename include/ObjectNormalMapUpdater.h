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

		struct NormalMapResult {
			std::uint32_t slot;
			RE::BSGeometry* geometry; //for ptr compare only 
			std::string geoName;
			std::string textureName;
			std::uint32_t vertexCount;
			RE::NiPointer<RE::NiSourceTexture> normalmap;
		};
		typedef concurrency::concurrent_vector<NormalMapResult> UpdateResult;
		UpdateResult UpdateObjectNormalMap(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet);
		UpdateResult UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet);

	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const PlayerCellChangeEvent& e) override;

	private:
		struct TextureResourceData {
			std::string textureName;
			std::clock_t time = -1;

			Microsoft::WRL::ComPtr<ID3D11Query> query = nullptr;
			bool GetQuery(ID3D11Device* device, ID3D11DeviceContext* context) {
				if (!device || !context)
					return false;

				D3D11_QUERY_DESC queryDesc = {};
				queryDesc.Query = D3D11_QUERY_EVENT;
				queryDesc.MiscFlags = 0;

				HRESULT hr = device->CreateQuery(&queryDesc, &query);
				if (FAILED(hr)) {
					query = nullptr;
					return false;
				}
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->End(query.Get());
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				return true;
			}
			bool IsQueryDone(ID3D11DeviceContext* context) {
				if (!context || !query)
					return true; 
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				HRESULT hr = context->GetData(query.Get(), nullptr, 0, 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				if (FAILED(hr))
					return true;
				return hr == S_OK;
			}

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
			Microsoft::WRL::ComPtr<ID3D11Texture2D> dstCompressTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> dstCompressShaderResourceView = nullptr;

			struct BleedTextureData {
				std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D_1 = nullptr;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D_2 = nullptr;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv1 = nullptr;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv2 = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav1 = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav2 = nullptr;
			};
			BleedTextureData bleedTextureData;

			struct MergeTextureData {
				std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2Dalt = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav = nullptr;
			};
			MergeTextureData mergeTextureData;

			struct TexturePostProcessingData {
				std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffer;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav = nullptr;
			};
			TexturePostProcessingData texturePostProcessingData;
		};
		typedef std::shared_ptr<TextureResourceData> TextureResourceDataPtr;

		bool IsDetailNormalMap(std::string a_normalMapPath);

		bool ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
		bool CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		bool IsValidPixel(const std::uint32_t a_pixel);
		bool BleedTexture(TextureResourceDataPtr& resourceData, std::int32_t margin, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool BleedTextureGPU(TextureResourceDataPtr& resourceData, std::int32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool TexturePostProcessingGPU(TextureResourceDataPtr& resourceData, std::uint32_t blurRadius, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskSrv, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool MergeTexture(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11Texture2D> srvTex);
		bool MergeTextureGPU(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D> srvTex);

		bool CopySubresourceRegion(Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, UINT dstMipMapLevel, UINT srcMipMapLevel);
		bool CompressTexture(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srcSrv);
		bool CompressTextureBC7(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srcSrv);
		
		void GPUPerformanceLog(std::string funcStr, bool isEnd, bool isAverage = true, std::uint32_t args = 0);
		void WaitForGPU();

		const std::string_view BleedTextureShaderName = "BleedTexture";
		const std::string_view UpdateNormalMapShaderName = "UpdateNormalMap";
		const std::string_view MergeTextureShaderName = "MergeTexture";
		const std::string_view TexturePostProcessingShaderName = "TexturePostProcessing";

		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState = nullptr;

		struct GeometryResourceData {
			std::clock_t time = -1;

			Microsoft::WRL::ComPtr<ID3D11Query> query = nullptr;
			bool GetQuery(ID3D11Device* device, ID3D11DeviceContext* context) {
				if (!device || !context)
					return false;

				D3D11_QUERY_DESC queryDesc = {};
				queryDesc.Query = D3D11_QUERY_EVENT;
				queryDesc.MiscFlags = 0;

				HRESULT hr = device->CreateQuery(&queryDesc, &query);
				if (FAILED(hr)) {
					query = nullptr;
					return false;
				}
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				context->End(query.Get());
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				return true;
			}
			bool IsQueryDone(ID3D11DeviceContext* context) {
				if (!context)
					return true;
				Shader::ShaderManager::GetSingleton().ShaderContextLock();
				HRESULT hr = context->GetData(query.Get(), nullptr, 0, 0);
				Shader::ShaderManager::GetSingleton().ShaderContextUnlock();
				if (FAILED(hr))
					return true;
				if (hr == S_OK)
					return true;
				return false;
			}
			void clear() {
				query.Reset();
				vertexBuffer.Reset();
				vertexSRV.Reset();
				uvBuffer.Reset();
				uvSRV.Reset();
				normalBuffer.Reset();
				normalSRV.Reset();
				tangentBuffer.Reset();
				tangentSRV.Reset();
				bitangentBuffer.Reset();
				bitangentSRV.Reset();
				indicesBuffer.Reset();
				indicesSRV.Reset();
			}

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

		std::shared_mutex ResourceDataMapLock;
		concurrency::concurrent_vector<TextureResourceDataPtr> ResourceDataMap;
	};
}