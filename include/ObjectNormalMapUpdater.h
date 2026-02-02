#pragma once

namespace Mus {
    class Ref {
    public:
        std::uint32_t DecRef() {
            std::atomic_ref rc(refCount);
            return --rc;
        }
        std::uint32_t IncRef() {
            std::atomic_ref rc(refCount);
            return ++rc;
        }
        constexpr std::uint32_t GetCount() const noexcept {
            return refCount;
        }

    private:
        volatile std::uint32_t refCount = 0;
    };

	class RefGuard {
    public:
        RefGuard() = delete;
        RefGuard(const RefGuard&) = delete;
        RefGuard(RefGuard&&) = delete;

        RefGuard(Ref* a_ref) noexcept : ref(a_ref) { ref->IncRef(); }
        ~RefGuard() noexcept { ref->DecRef(); }

        RefGuard& operator=(const RefGuard&) = delete;
        RefGuard& operator=(RefGuard&&) = delete;

    private:
        Ref* ref = nullptr;
	};

	class ObjectNormalMapUpdater : 
		public Ref,
		public IEventListener<FrameEvent>,
		public IEventListener<PlayerCellChangeEvent> {
	public:
		ObjectNormalMapUpdater() {};
		~ObjectNormalMapUpdater() {};

		[[nodiscard]] static ObjectNormalMapUpdater& GetSingleton() {
			static ObjectNormalMapUpdater instance;
			return instance;
		}

		bool Init();
		bool CreateGeometryResourceData(RE::FormID a_actorID, GeometryDataPtr a_data);

		void ClearGeometryResourceData();

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
		typedef std::vector<NormalMapResult> UpdateResult;
		UpdateResult UpdateObjectNormalMap(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet& a_updateSet);
		UpdateResult UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet& a_updateSet);

	protected:
		void onEvent(const FrameEvent& e) override;
		void onEvent(const PlayerCellChangeEvent& e) override;

	private:
		inline ID3D11Device* GetDevice(std::int8_t i = -1) const { 
			switch (i) {
            case 0:
                return Shader::ShaderManager::GetSingleton().GetDevice();
            case 1:
                return Shader::ShaderManager::GetSingleton().GetSecondDevice();
            default:
                break;
			}
            return isSecondGPUEnabled ? Shader::ShaderManager::GetSingleton().GetSecondDevice() : Shader::ShaderManager::GetSingleton().GetDevice();
		};
        inline ID3D11DeviceContext* GetContext(std::int8_t i = -1) const {
            switch (i) {
            case 0:
                return Shader::ShaderManager::GetSingleton().GetContext();
            case 1:
                return Shader::ShaderManager::GetSingleton().GetSecondContext();
            default:
                break;
            }
            return isSecondGPUEnabled ? Shader::ShaderManager::GetSingleton().GetSecondContext() : Shader::ShaderManager::GetSingleton().GetContext();
		};

		bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texOutput, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOutput);
		bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texOutput, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOutput);
		bool LoadTextureCPU(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filePath, D3D11_TEXTURE2D_DESC& texDesc, Microsoft::WRL::ComPtr<ID3D11Texture2D>& output);

		bool isValidGPU[2] = {false, false};

        const std::string_view UpdateNormalMapShaderName = "UpdateNormalMap";
        const std::string_view MergeTextureShaderName = "MergeTexture";
        const std::string_view GenerateMipsShaderName = "GenerateMips";

		Shader::ShaderManager::ComputeShader updateNormalMapShader[2] = {nullptr, nullptr};
        Shader::ShaderManager::ComputeShader mergeTexture[2] = {nullptr, nullptr};
        Shader::ShaderManager::ComputeShader generateMips[2] = {nullptr, nullptr};

        Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState[2] = {nullptr, nullptr};

		struct TextureResourceData {
			RE::BSGeometry* geometry;
			std::string textureName;
			std::clock_t time = -1;

			std::shared_ptr<Shader::ShaderLocker> sl;

			Microsoft::WRL::ComPtr<ID3D11Query> query = nullptr;
			bool GetQuery(ID3D11Device* device, ID3D11DeviceContext* context) {
				if (!device || !context)
					return false;
				
				sl = std::make_shared<Shader::ShaderLocker>(context);
				D3D11_QUERY_DESC queryDesc = {};
				queryDesc.Query = D3D11_QUERY_EVENT;
				queryDesc.MiscFlags = 0;

				HRESULT hr = device->CreateQuery(&queryDesc, &query);
				if (FAILED(hr)) {
					query = nullptr;
					return false;
				}
				sl->Lock();
				context->End(query.Get());
				sl->Unlock();
				queryContext = context;
				return true;
			}
			bool IsQueryDone() {
				if (!queryContext || !query)
					return true;
				sl->Lock();
				HRESULT hr = queryContext->GetData(query.Get(), nullptr, 0, 0);
				sl->Unlock();
				if (FAILED(hr))
					return true;
				return hr == S_OK;
			}
			ID3D11DeviceContext* queryContext = nullptr;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> srcTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> detailTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> detailShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> overlayTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> overlayShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> maskTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskShaderResourceView = nullptr;

			struct GenerateMipsData {
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = nullptr;
				std::vector<Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> uavs;
			};
			GenerateMipsData generateMipsData;

			struct TextureCompressData {
				Microsoft::WRL::ComPtr<ID3D11Texture2D> srcStagingTexture = nullptr;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> dstStagingTexture = nullptr;
			};
			TextureCompressData textureCompressData;

			struct CopySecondToMainData {
				Microsoft::WRL::ComPtr<ID3D11Texture2D> copyTexture2D = nullptr;
			};
			CopySecondToMainData copySecondToMainData;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture2D = nullptr;
		};
		typedef std::shared_ptr<TextureResourceData> TextureResourceDataPtr;
        typedef std::vector<TextureResourceDataPtr> ResourceDatas;
        typedef std::unordered_set<RE::BSGeometry*> MergedTextureGeometries;

        struct UpdateNormalMapBufferData {
            UINT texWidth;
            UINT texHeight;
            UINT indicesStart;
            UINT indicesEnd;

            UINT hasSrcTexture;
            UINT hasDetailTexture;
            UINT hasOverlayTexture;
            UINT hasMaskTexture;

            UINT tangentZCorrection;
            float detailStrength;
            UINT padding1;
            UINT padding2;
        };
        static_assert(sizeof(UpdateNormalMapBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");
        Microsoft::WRL::ComPtr<ID3D11Buffer> updateNormalMapBuffer[2] = {nullptr, nullptr};

		struct MergeTextureBufferData {
            UINT width;
            UINT height;
            UINT widthStart;
            UINT heightStart;
        };
        static_assert(sizeof(MergeTextureBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");
        Microsoft::WRL::ComPtr<ID3D11Buffer> mergeTextureBuffer[2] = {nullptr, nullptr};

		struct GenerateMipsBufferData {
            UINT width;
            UINT height;
            UINT widthStart;
            UINT heightStart;

            UINT mipLevel;
            UINT padding1;
            UINT srcWidth;
            UINT srcHeight;
        };
        static_assert(sizeof(GenerateMipsBufferData) % 16 == 0, "Constant buffer must be 16-byte aligned.");
        Microsoft::WRL::ComPtr<ID3D11Buffer> generateMipsBuffer[2] = {nullptr, nullptr};

		class ShaderBackup {
        public:
            virtual void Backup(ID3D11DeviceContext* context) = 0;
            virtual void Revert(ID3D11DeviceContext* context) = 0;
		};
		class ShaderBackupGuard {
        public:
            ShaderBackupGuard() = delete;
            ShaderBackupGuard(const ShaderBackupGuard&) = delete;
            ShaderBackupGuard(ShaderBackupGuard&&) = delete;

            ShaderBackupGuard(ID3D11DeviceContext* a_context, ShaderBackup& a_shaderBackup) noexcept
				: context(a_context), shaderBackup(a_shaderBackup) { shaderBackup.Backup(context); };
            ~ShaderBackupGuard() noexcept { shaderBackup.Revert(context); };

            ShaderBackupGuard& operator=(const ShaderBackupGuard&) = delete;
            ShaderBackupGuard& operator=(ShaderBackupGuard&&) = delete;

		private:
            ID3D11DeviceContext* context = nullptr;
            ShaderBackup& shaderBackup;
		};

		class UpdateNormalMapBackup : public ShaderBackup {
        public:
            void Backup(ID3D11DeviceContext* context) override {
                context->CSGetShader(&shader, nullptr, 0);
                context->CSGetConstantBuffers(0, 1, &constBuffer);
                context->CSGetShaderResources(0, 1, &vertexSRV);
                context->CSGetShaderResources(1, 1, &uvSRV);
                context->CSGetShaderResources(2, 1, &normalSRV);
                context->CSGetShaderResources(3, 1, &tangentSRV);
                context->CSGetShaderResources(4, 1, &bitangentSRV);
                context->CSGetShaderResources(5, 1, &indicesSRV);
                context->CSGetShaderResources(6, 1, &srcSRV);
                context->CSGetShaderResources(7, 1, &detailSRV);
                context->CSGetShaderResources(8, 1, &overlaySRV);
                context->CSGetShaderResources(9, 1, &maskSRV);
                context->CSGetUnorderedAccessViews(0, 1, &dstUAV);
                context->CSGetSamplers(0, 1, &samplerState);
                Unbind(context);
            }
            void Revert(ID3D11DeviceContext* context) override {
                Unbind(context);
                context->CSSetShader(shader.Get(), nullptr, 0);
                context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
                context->CSSetShaderResources(0, 1, vertexSRV.GetAddressOf());
                context->CSSetShaderResources(1, 1, uvSRV.GetAddressOf());
                context->CSSetShaderResources(2, 1, normalSRV.GetAddressOf());
                context->CSSetShaderResources(3, 1, tangentSRV.GetAddressOf());
                context->CSSetShaderResources(4, 1, bitangentSRV.GetAddressOf());
                context->CSSetShaderResources(5, 1, indicesSRV.GetAddressOf());
                context->CSSetShaderResources(6, 1, srcSRV.GetAddressOf());
                context->CSSetShaderResources(7, 1, detailSRV.GetAddressOf());
                context->CSSetShaderResources(8, 1, overlaySRV.GetAddressOf());
                context->CSSetShaderResources(9, 1, maskSRV.GetAddressOf());
                context->CSSetUnorderedAccessViews(0, 1, dstUAV.GetAddressOf(), nullptr);
                context->CSSetSamplers(0, 1, samplerState.GetAddressOf());
            }

        private:
            void Unbind(ID3D11DeviceContext* context) {
                ID3D11Buffer* emptyBuffer[1] = {nullptr};
                ID3D11ShaderResourceView* emptySRV[10] = {nullptr};
                ID3D11UnorderedAccessView* emptyUAV[1] = {nullptr};
                ID3D11SamplerState* emptySamplerState[1] = {nullptr};

                context->CSSetShader(nullptr, nullptr, 0);
                context->CSSetConstantBuffers(0, 1, emptyBuffer);
                context->CSSetShaderResources(0, 10, emptySRV);
                context->CSSetUnorderedAccessViews(0, 1, emptyUAV, nullptr);
                context->CSSetSamplers(0, 1, emptySamplerState);
            }

            Shader::ShaderManager::ComputeShader shader;
            Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vertexSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uvSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tangentSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bitangentSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> indicesSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> detailSRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> overlaySRV;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskSRV;
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstUAV;
            Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
        };

		class MergeTextureBackup : public ShaderBackup {
        public:
            void Backup(ID3D11DeviceContext* context) override {
                context->CSGetShader(&shader, nullptr, 0);
                context->CSGetConstantBuffers(0, 1, &constBuffer);
                context->CSGetShaderResources(0, 1, &srv);
                context->CSGetUnorderedAccessViews(0, 1, &uav);
                Unbind(context);
            }
            void Revert(ID3D11DeviceContext* context) override {
                Unbind(context);
                context->CSSetShader(shader.Get(), nullptr, 0);
                context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
                context->CSSetShaderResources(0, 1, srv.GetAddressOf());
                context->CSSetUnorderedAccessViews(0, 1, uav.GetAddressOf(), nullptr);
            }

        private:
            void Unbind(ID3D11DeviceContext* context) {
                ID3D11Buffer* emptyBuffer[1] = {nullptr};
                ID3D11ShaderResourceView* emptySRV[1] = {nullptr};
                ID3D11UnorderedAccessView* emptyUAV[1] = {nullptr};
                context->CSSetShader(nullptr, nullptr, 0);
                context->CSSetConstantBuffers(0, 1, emptyBuffer);
                context->CSSetShaderResources(0, 1, emptySRV);
                context->CSSetUnorderedAccessViews(0, 1, emptyUAV, nullptr);
            }

            Shader::ShaderManager::ComputeShader shader;
            Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
        };

		class GenerateMipsBackup : public ShaderBackup {
        public:
            void Backup(ID3D11DeviceContext* context) {
                context->CSGetShader(&shader, nullptr, 0);
                context->CSGetConstantBuffers(0, 1, &constBuffer);
                context->CSGetShaderResources(0, 1, &src0);
                context->CSGetUnorderedAccessViews(0, 1, &dst);
                context->CSGetUnorderedAccessViews(1, 1, &src);
                Unbind(context);
            }
            void Revert(ID3D11DeviceContext* context) {
                Unbind(context);
                context->CSSetShader(shader.Get(), nullptr, 0);
                context->CSSetConstantBuffers(0, 1, constBuffer.GetAddressOf());
                context->CSSetShaderResources(0, 1, src0.GetAddressOf());
                context->CSSetUnorderedAccessViews(0, 1, dst.GetAddressOf(), nullptr);
                context->CSSetUnorderedAccessViews(1, 1, src.GetAddressOf(), nullptr);
            }

        private:
            void Unbind(ID3D11DeviceContext* context) {
                ID3D11Buffer* emptyBuffer[1] = {nullptr};
                ID3D11ShaderResourceView* emptySRV[1] = {nullptr};
                ID3D11UnorderedAccessView* emptyUAV[2] = {nullptr};
                context->CSSetShader(nullptr, nullptr, 0);
                context->CSSetConstantBuffers(0, 1, emptyBuffer);
                context->CSSetShaderResources(0, 1, emptySRV);
                context->CSSetUnorderedAccessViews(0, 2, emptyUAV, nullptr);
            }

            Shader::ShaderManager::ComputeShader shader;
            Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> src0;
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dst;
            Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> src;
        };

		bool IsDetailNormalMap(const std::string& a_normalMapPath);
        void LoadCacheResource(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet& a_updateSet, MergedTextureGeometries& mergedTextureGeometries, ResourceDatas& resourceDatas, UpdateResult& results);

		DirectX::XMVECTOR SlerpVector(const DirectX::XMVECTOR& a, const DirectX::XMVECTOR& b, const float& t);
		bool ComputeBarycentric(const float& px, const float& py, const DirectX::XMINT2& a, const DirectX::XMINT2& b, const DirectX::XMINT2& c, DirectX::XMFLOAT3& out);
        bool CreateConstBuffer(ID3D11Device* device, UINT byteWidth, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut);
		bool CreateStructuredBuffer(ID3D11Device* device, const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		bool CopySubresourceRegion(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* dstTexture, ID3D11Texture2D* srcTexture, UINT dstMipMapLevel, UINT srcMipMapLevel);
		bool CopySubresourceRegion(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* dstTexture, ID3D11Texture2D* srcTexture);
		bool CopySubresourceFromBuffer(ID3D11Device* device, ID3D11DeviceContext* context, std::vector<std::uint8_t>& buffer, UINT rowPitch, UINT mipLevel, ID3D11Texture2D* dstTexture);
		bool CopySubresourceFromBuffer(ID3D11Device* device, ID3D11DeviceContext* context, std::vector<std::vector<std::uint8_t>>& buffer, std::vector<UINT>& rowPitch, ID3D11Texture2D* dstTexture);

		void PostProcessing(ID3D11Device* device, ID3D11DeviceContext* context, ResourceDatas& resourceDatas, UpdateResult& results, MergedTextureGeometries& mergedTextureGeometries);
        void PostProcessingGPU(ID3D11Device* device, ID3D11DeviceContext* context, ResourceDatas& resourceDatas, UpdateResult& results, MergedTextureGeometries& mergedTextureGeometries);

		bool MergeTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& rsourceData, ID3D11Texture2D* dstTex, ID3D11Texture2D* srcTex);
        bool MergeTextureGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11UnorderedAccessView* dstUAV, ID3D11Texture2D* dstTex, ID3D11ShaderResourceView* srcSRV);

		bool GenerateMips(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11Texture2D* texInOut);
		bool GenerateMipsGPU(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, ID3D11ShaderResourceView* srvInOut, ID3D11Texture2D* texInOut);

		bool CompressTexture(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool CompressTextureBC7(ID3D11Device* device, ID3D11DeviceContext* context, TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);

		bool CopyResourceSecondToMain(TextureResourceDataPtr& resourceData, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut);
		
		void GPUPerformanceLog(ID3D11Device* device, ID3D11DeviceContext* context, std::string funcStr, bool isEnd, bool isAverage = true, std::uint32_t args = 0);

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
        typedef std::unordered_map<RE::FormID, GeometryResourceDataPtr> GeometryResourceDataMap;
        std::shared_mutex geometryResourceDataMapLock;
        GeometryResourceDataMap geometryResourceDataMap;
		GeometryResourceDataPtr GetGeometryResourceData(RE::FormID a_actorID);

		std::shared_mutex resourceDataMapLock;
        typedef std::vector<TextureResourceDataPtr> ResourceDataMap;
        ResourceDataMap resourceDataMap;

		std::uint64_t GetHash(UpdateTextureSet updateSet, std::uint64_t geoHash);
	};

	class WaitForGPU {
	public:
		WaitForGPU() = delete;
		WaitForGPU(ID3D11Device* a_device, ID3D11DeviceContext* a_context);
		~WaitForGPU() {};

		void Wait();

	private:
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;
		Microsoft::WRL::ComPtr<ID3D11Query> query = nullptr;
		Shader::ShaderLocker sl;
	};
}