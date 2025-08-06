#pragma once

namespace Mus {
	typedef std::uint32_t bSlot; //RE::BIPED_OBJECT
	typedef std::uint32_t bSlotbit; //1 << bSlot

	struct TaskID {
		RE::FormID refrID;
		std::string taskName;
		std::int64_t taskID;
	};

	struct UpdateTextureSet {
		bSlot slot = 0;

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
		bSlot bipedSlot;
		std::string texturePath;
	};
}