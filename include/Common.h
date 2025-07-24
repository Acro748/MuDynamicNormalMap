#pragma once

namespace Mus {
	struct TaskID {
		RE::FormID refrID;
		std::string taskName;
		std::int64_t taskID;
	};

	struct BakeTextureSet {
		std::string geometryName;
		std::string textureName;
		std::string srcTexturePath;
		std::string detailTexturePath;
		std::string overlayTexturePath;
		std::string maskTexturePath;
		float detailStrength = 0.5f;
	};
	typedef concurrency::concurrent_unordered_map<RE::BSGeometry*, BakeTextureSet> BakeSet;

	struct TextureInfo {
		RE::FormID actorID;
		RE::FormID armorID;
		std::uint32_t bipedSlot;
		std::string geoName;
		std::uint32_t vertexCount;
	};
}