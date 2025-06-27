#pragma once

namespace Mus {
	class ObjectNormalMapBaker {
	public:
		ObjectNormalMapBaker() {};
		~ObjectNormalMapBaker() {};

		[[nodiscard]] static ObjectNormalMapBaker& GetSingleton() {
			static ObjectNormalMapBaker instance;
			return instance;
		}

		struct NormalMapResult {
			std::size_t index;
			std::string geoName;
			std::string textureName;
			std::size_t vertexCount;
			RE::NiPointer<RE::NiSourceTexture> normalmap;
		};
		typedef concurrency::concurrent_vector<NormalMapResult> BakeResult;
		BakeResult BakeObjectNormalMap(TaskID taskID, GeometryData a_data, std::unordered_map<std::size_t, BakeTextureSet> a_bakeSet);
		RE::NiPointer<RE::NiSourceTexture> BakeObjectNormalMapGPU(TaskID taskID, std::string textureName, GeometryData a_data, std::string a_srcTexturePath, std::string a_maskTexturePath);

	private:
		struct TileTriangleRange {
			std::uint32_t startOffset;
			std::uint32_t count;
		};
		struct TileInfo {
			std::uint32_t TILE_SIZE;
			std::uint32_t TEX_WIDTH;
			std::uint32_t TEX_HEIGHT;
			std::uint32_t TILE_COUNT_X() { return this->TEX_WIDTH / this->TILE_SIZE; };
			std::uint32_t TILE_COUNT_Y() { return this->TEX_HEIGHT / this->TILE_SIZE; };
			std::uint32_t TILE_COUNT() { return this->TILE_COUNT_X() * this->TILE_COUNT_Y(); };
		};

		bool ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
		bool ComputeBarycentrics(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, std::int32_t margin, DirectX::XMFLOAT3& out);
		bool ComputeBarycentrics(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
		void GenerateTileTriangleRanges(TileInfo tileInfo, const GeometryData& a_data, std::vector<uint32_t>& outPackedTriangleIndices, std::vector<TileTriangleRange>& outTileRanges);
		bool CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		std::string GetTangentNormalMapPath(std::string a_normalMapPath);
		std::string GetOverlayNormalMapPath(std::string a_normalMapPath);
		bool IsInvalidPixel(const std::uint32_t a_pixel);
		void BleedTexture(std::uint32_t* pixels, UINT width, UINT height, std::int32_t margin);
	};
}