#pragma once

namespace Mus {
	class ObjectNormalMapUpdater {
	public:
		ObjectNormalMapUpdater() {};
		~ObjectNormalMapUpdater() {};

		[[nodiscard]] static ObjectNormalMapUpdater& GetSingleton() {
			static ObjectNormalMapUpdater instance;
			return instance;
		}

		void Init();

		struct NormalMapResult {
			bSlot slot;
			RE::BSGeometry* geometry = nullptr; //for ptr compare only 
			std::string geoName = "";
			std::string texturePath = "";
			std::string textureName = "";
			std::uint32_t vertexCount = 0;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> normalmapTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalmapShaderResourceView = nullptr;
		};
		typedef std::shared_ptr<NormalMapResult> NormalMapResultPtr;
		typedef concurrency::concurrent_vector<NormalMapResultPtr> UpdateResult;
		UpdateResult UpdateObjectNormalMap(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet);
		UpdateResult UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryDataPtr a_data, UpdateSet a_updateSet);

	private:
		bool IsDetailNormalMap(const std::string& a_normalMapPath);

		DirectX::XMVECTOR SlerpVector(const DirectX::XMVECTOR& a, const DirectX::XMVECTOR& b, const float& t);
		bool ComputeBarycentric(const float& px, const float& py, const DirectX::XMINT2& a, const DirectX::XMINT2& b, const DirectX::XMINT2& c, DirectX::XMFLOAT3& out);
		bool CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		bool IsValidPixel(const std::uint32_t a_pixel);
		bool BleedTexture(const std::string& textureName, std::int32_t margin, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool BleedTextureGPU(const std::string& textureName, std::int32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool MergeTexture(const std::string& textureName, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11Texture2D> srvTex);
		bool MergeTextureGPU(const std::string& textureName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTex, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D> srvTex);

		bool CopySubresourceRegion(Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, UINT dstMipMapLevel, UINT srcMipMapLevel);
		bool CompressTexture(const std::string& textureName, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srcSrv);
		bool CompressTextureBC7(const std::string& textureName, Microsoft::WRL::ComPtr<ID3D11Texture2D>& dstTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& dstSrv, Microsoft::WRL::ComPtr<ID3D11Texture2D>& srcTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srcSrv);
		
		void GPUPerformanceLog(std::string funcStr, bool isEnd, bool isAverage = true, std::uint32_t args = 0);
		void WaitForGPU();

		const std::string_view BleedTextureShaderName = "BleedTexture";
		const std::string_view UpdateNormalMapShaderName = "UpdateNormalMap";
		const std::string_view MergeTextureShaderName = "MergeTexture";
		const std::string_view TexturePostProcessingShaderName = "TexturePostProcessing";

		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState = nullptr;
	};
}