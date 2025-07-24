#pragma once

#include "IPluginInterface.h"
#include "StringTable.h"

class SliderInternal
{
public:
	enum
	{
		kCategoryExpressions = 1024,
		kCategoryExtra = 512,
		kCategoryBody = 4,
		kCategoryHead = 8,
		kCategoryFace = 16,
		kCategoryEyes = 32,
		kCategoryBrow = 64,
		kCategoryMouth = 128,
		kCategoryHair = 256
	};

	enum
	{
		kTypeSlider = 0,
		kTypePreset = 1,
		kTypeHeadPart = 2
	};

	std::int32_t			category;
	SKEEFixedString	name;
	SKEEFixedString	lowerBound;
	SKEEFixedString	upperBound;
	std::uint8_t			type;
	std::uint8_t			presetCount;
};
typedef std::shared_ptr<SliderInternal> SliderInternalPtr;
typedef std::vector<SliderInternalPtr> SliderList;
typedef std::unordered_map<RE::TESRace*, SliderList> RaceSliders;
typedef std::vector<SKEEFixedString> MorphSet;

class MorphMap : public std::map<SKEEFixedString, MorphSet>
{
public:
	class Visitor
	{
	public:
		virtual bool Accept(const SKEEFixedString& morphName) { return false; };
	};
};

class SliderGender
{
public:
	SliderInternalPtr slider[2];
};
typedef std::shared_ptr<SliderGender> SliderGenderPtr;

class SliderMap : public std::unordered_map<SKEEFixedString, SliderGenderPtr>
{
public:
};
typedef std::shared_ptr<SliderMap>	SliderMapPtr;

class SliderSet : public std::set<SliderMapPtr>
{
public:
};
typedef std::shared_ptr<SliderSet> SliderSetPtr;

class RaceMap : public std::unordered_map<RE::TESRace*, SliderSetPtr>
{
public:
};

class ValueSet : public std::unordered_map<StringTableItem, float>
{
public:
};

class ValueMap : public std::unordered_map<RE::TESNPC*, ValueSet>
{
public:
};

class TRIFile
{
public:
	struct Morph
	{
		SKEEFixedString name;
		float multiplier;

		struct Vertex
		{
			std::int16_t x, y, z;
		};

		std::vector<Vertex> vertices;
	};

	std::int32_t vertexCount;
	std::unordered_map<SKEEFixedString, Morph> morphs;
};

class TRIModelData
{
public:
	TRIModelData()
	{
		vertexCount = -1;
		morphModel = NULL;
	}
	std::int32_t vertexCount;
	std::shared_ptr<TRIFile> triFile;
	RE::TESModelTri* morphModel;
};
typedef std::unordered_map<SKEEFixedString, TRIModelData> ModelMap;

class MappedSculptData : public std::unordered_map<std::uint16_t, RE::NiPoint3>
{
public:
};
typedef std::shared_ptr<MappedSculptData> MappedSculptDataPtr;

class SculptData : public std::unordered_map<StringTableItem, MappedSculptDataPtr >
{
public:
};
typedef std::shared_ptr<SculptData> SculptDataPtr;

class PresetData
{
public:
	struct Tint
	{
		std::uint32_t index;
		std::uint32_t color;
		SKEEFixedString name;
	};

	struct Morph
	{
		float value;
		SKEEFixedString name;
	};

	struct Texture
	{
		std::uint8_t index;
		SKEEFixedString name;
	};

	float weight;
	std::uint32_t hairColor;
	std::vector<std::string> modList;
	std::vector<RE::BGSHeadPart*> headParts;
	std::vector<std::int32_t> presets;
	std::vector<float> morphs;
	std::vector<Tint> tints;
	std::vector<Morph> customMorphs;
	std::vector<Texture> faceTextures;
	RE::BGSTextureSet* headTexture;
	RE::BSFixedString tintTexture;
	typedef std::map<SKEEFixedString, std::vector<OverrideVariant>> OverrideData;
	OverrideData overrideData;
	typedef std::map<std::uint32_t, std::vector<OverrideVariant>> SkinData;
	SkinData skinData[2];
	typedef std::map<SKEEFixedString, std::map<SKEEFixedString, std::vector<OverrideVariant>>> TransformData;
	TransformData transformData[2];
	SculptDataPtr sculptData;
	typedef std::unordered_map<SKEEFixedString, std::unordered_map<SKEEFixedString, float>> BodyMorphData;
	BodyMorphData bodyMorphData;
};
typedef std::shared_ptr<PresetData> PresetDataPtr;
typedef std::unordered_map<RE::TESNPC*, PresetDataPtr> PresetMap;

class SculptStorage : public std::unordered_map<RE::TESNPC*, SculptDataPtr>
{
public:
};

class FaceMorphInterfaceSE : public IPluginInterface //AE that lower than 1.6.640 and SE, VR
{
public:
	virtual bool Load(SKSE::SerializationInterface* intfc, std::uint32_t version) = 0;
	virtual void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion) = 0;
	virtual void Revert() = 0;

	virtual float GetMorphValueByName(RE::TESNPC* npc, const SKEEFixedString& name) = 0;
	virtual void SetMorphValue(RE::TESNPC* npc, const SKEEFixedString& name, float value) = 0;

protected:
	SliderList* currentList;
	RaceSliders m_internalMap;
	RaceMap m_raceMap;
	MorphMap m_morphMap;
	ValueMap m_valueMap;
	ModelMap m_modelMap;
	PresetMap m_mappedPreset;

	SculptStorage m_sculptStorage;
};


class FaceMorphInterfaceAE : public IPluginInterface //1.6.640 or higher
{
public:
	virtual uint32_t GetVersion() = 0;
	virtual void Revert() = 0;

	virtual float GetMorphValueByName(RE::TESNPC* npc, const SKEEFixedString& name) = 0;
	virtual void SetMorphValue(RE::TESNPC* npc, const SKEEFixedString& name, float value) = 0;

protected:
	SliderList* currentList;
	RaceSliders m_internalMap;
	RaceMap m_raceMap;
	MorphMap m_morphMap;
	ValueMap m_valueMap;
	ModelMap m_modelMap;

	SculptStorage m_sculptStorage;

	friend class RacePartDefaultGen;
};