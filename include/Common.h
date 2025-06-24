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
		std::string maskTexturePath;
	};

	struct BakeData {
		GeometryData geoData;
		std::unordered_map<std::size_t, BakeTextureSet> bakeTextureSet;
	};

	struct TextureInfo {
		RE::FormID actorID;
		RE::FormID armorID;
		std::uint32_t bipedSlot;
		std::string geoName;
		std::uint32_t vertexCount;
	};


	inline RE::NiPointer<RE::NiSkinPartition> GetSkinPartition(RE::BSGeometry* a_geo)
	{
		if (!a_geo)
			return nullptr;
		if (!a_geo->GetGeometryRuntimeData().skinInstance)
			return nullptr;
		RE::NiSkinInstance* skinInstance = a_geo->GetGeometryRuntimeData().skinInstance.get();
		if (!skinInstance->skinPartition)
			return nullptr;
		return skinInstance->skinPartition;
	}
}