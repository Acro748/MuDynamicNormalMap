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

		struct NormalMapResult {
			RE::BSGeometry* geometry;
			std::size_t index;
			std::string geoName;
			std::string textureName;
			std::size_t vertexCount;
			RE::NiPointer<RE::NiSourceTexture> normalmap;
		};
		typedef concurrency::concurrent_vector<NormalMapResult> BakeResult;
		BakeResult UpdateObjectNormalMap(TaskID taskID, GeometryData a_data, std::unordered_map<std::size_t, BakeTextureSet> a_bakeSet);
		BakeResult UpdateObjectNormalMapGPU(TaskID taskID, GeometryData a_data, std::unordered_map<std::size_t, BakeTextureSet> a_bakeSet);

	private:
		bool IsTangentNormalMap(std::string a_normalMapPath);

		bool ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
		bool ComputeBarycentrics(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, std::int32_t margin, DirectX::XMFLOAT3& out);
		bool ComputeBarycentrics(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
		bool CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		bool IsValidPixel(const std::uint32_t a_pixel);
		bool BleedTexture(std::uint8_t* pData, UINT width, UINT height, UINT RowPitch, std::uint32_t margin);
		bool BleedTextureGPU(TaskID taskID, std::uint32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);

		const std::string_view BleedTextureShaderName = "BleedTexture";
		const std::string_view UpdateObjectNormalMapShaderName = "UpdateObjectNormalMap";
	};
}