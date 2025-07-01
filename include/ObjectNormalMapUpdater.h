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
		void GenerateTileTriangleRanges(TileInfo tileInfo, const GeometryData& a_data, const std::size_t indicesStartOffset, const std::size_t indicesEndOffset, std::vector<uint32_t>& outPackedTriangleIndices, std::vector<TileTriangleRange>& outTileRanges);
		bool CreateStructuredBuffer(const void* data, UINT size, UINT stride, Microsoft::WRL::ComPtr<ID3D11Buffer>& bufferOut, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvOut);
		std::string GetTangentNormalMapPath(std::string a_normalMapPath);
		bool IsValidPixel(const std::uint32_t a_pixel);
		bool BleedTexture(std::uint8_t* pData, UINT width, UINT height, UINT RowPitch, std::uint32_t margin);
		bool BleedTextureGPU(TaskID taskID, std::uint32_t margin, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srvInOut, Microsoft::WRL::ComPtr<ID3D11Texture2D>& texInOut);

		const std::string_view BleedTextureShaderName = "BleedTexture";
	};
}