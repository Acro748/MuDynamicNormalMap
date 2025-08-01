#pragma once

namespace Mus {
	struct TaskID {
		RE::FormID refrID;
		std::string taskName;
		std::int64_t taskID;
	};

	struct UpdateTextureSet {
		std::uint32_t slot = 0;

		std::string geometryName;
		std::string textureName;
		std::string srcTexturePath;
		std::string detailTexturePath;
		std::string overlayTexturePath;
		std::string maskTexturePath;
		float detailStrength = 0.5f;
	};
	typedef concurrency::concurrent_unordered_map<RE::BSGeometry*, UpdateTextureSet> UpdateSet;

	struct TextureInfo {
		RE::FormID actorID;
		std::uint32_t bipedSlot;
		std::string texturePath;
	};
}