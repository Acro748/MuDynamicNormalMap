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

		bool CreateGeometryResourceData(RE::FormID a_actorID, GeometryData& a_data);

		struct NormalMapResult {
			RE::BSGeometry* geometry;
			std::string geoName;
			std::string textureName;
			std::size_t vertexCount;
			RE::NiPointer<RE::NiSourceTexture> normalmap;
		};
		typedef concurrency::concurrent_vector<NormalMapResult> BakeResult;
		BakeResult UpdateObjectNormalMap(RE::FormID a_actorID, GeometryData& a_data, BakeSet& a_bakeSet);
		BakeResult UpdateObjectNormalMapGPU(RE::FormID a_actorID, GeometryData& a_data, BakeSet& a_bakeSet);
	private:
		bool IsDetailNormalMap(std::string a_normalMapPath);

		bool ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
		bool CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		bool IsValidPixel(const std::uint32_t a_pixel);
		bool BleedTexture(std::uint8_t* pData, UINT width, UINT height, UINT RowPitch, std::uint32_t margin);
		bool BleedTextureGPU(std::string textureName, std::uint32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);
		bool TexturePostProcessingGPU(std::string textureName, float threshold, float blendStrength, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);

		void GPUPerformanceLog(std::string funcStr, bool isEnd, bool isAverage = true, std::uint32_t args = 0);
		void WaitForGPU();

		const std::string_view BleedTextureShaderName = "BleedTexture";
		const std::string_view UpdateNormalMapShaderName = "UpdateNormalMap";
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
		concurrency::concurrent_unordered_map<RE::FormID, GeometryResourceData> GeometryResourceDataMap;

		struct TextureResourceData {
			void clear() {
				constBuffers.clear();
				srcShaderResourceView.Reset();
				detailShaderResourceView.Reset();
				overlayShaderResourceView.Reset();
				maskShaderResourceView.Reset();
				dstWriteTexture2D.Reset();
				dstWriteTextureUAV.Reset();
				pixelBuffer.Reset();
				pixelBufferUAV.Reset();
				bleedTextureData.clear();
				texturePostProcessingData.clear();
			}
			std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srcShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> detailShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> overlayShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskShaderResourceView = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Texture2D> dstWriteTexture2D = nullptr;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> dstWriteTextureUAV = nullptr;
			Microsoft::WRL::ComPtr<ID3D11Buffer> pixelBuffer = nullptr;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pixelBufferUAV = nullptr;

			struct BleedTextureData {
				void clear() {
					constBuffers.clear();
					texture2D.Reset();
					uav.Reset();
				}
				std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> constBuffers;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav = nullptr;
			};
			BleedTextureData bleedTextureData;

			struct TexturePostProcessingData {
				void clear() {
					constBuffer.Reset();
					texture2D.Reset();
					uav.Reset();
				}
				Microsoft::WRL::ComPtr<ID3D11Buffer> constBuffer;
				Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2D = nullptr;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav = nullptr;
			};
			TexturePostProcessingData texturePostProcessingData;
		};
		concurrency::concurrent_unordered_map<std::string, TextureResourceData> ResourceDataMap; //textureName, TextureResourceData
	};
}