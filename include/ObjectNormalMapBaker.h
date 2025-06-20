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

		RE::NiPointer<RE::NiSourceTexture> BakeObjectNormalMap(TaskID taskIDsrc, std::string textureName, GeometryData a_data, std::string a_srcTexturePath, std::string a_maskTexturePath);

	private:
		void RecalculateNormals(GeometryData& a_data, float a_smooth = 60.0f);
		void Subdivision(GeometryData& a_data, std::uint32_t a_subCount = 1);
		void VertexSmooth(GeometryData& a_data, float a_strength = 0.5f, std::uint32_t a_smoothCount = 1);
		bool ComputeBarycentric(float px, float py, DirectX::XMINT2 a, DirectX::XMINT2 b, DirectX::XMINT2 c, DirectX::XMFLOAT3& out);
	};
}