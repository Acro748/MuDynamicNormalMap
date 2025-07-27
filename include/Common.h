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
		RE::FormID armorID;
		std::uint32_t bipedSlot;
		std::string geoName;
		std::uint32_t vertexCount;
	};

	struct Pair3232Key {
		std::uint32_t first;
		std::uint32_t second;
		bool operator==(const Pair3232Key& p) const noexcept {
			return first == p.first && second == p.second;
		}
	};
	struct Pair3232Hash {
		std::size_t operator()(const Pair3232Key& p) const noexcept {
			return ((std::size_t)p.first << 32) | p.second;
		}
	};
}