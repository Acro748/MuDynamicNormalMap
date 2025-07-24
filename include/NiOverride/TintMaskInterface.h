#pragma once

#include "IPluginInterface.h"
#include "StringTable.h"

#include <unordered_map>
#include <functional>
#include <mutex>

class ItemAttributeData
{
public:
	using TextureMap = std::map<std::int32_t, StringTableItem>;
	using ColorMap = std::map<std::int32_t, std::uint32_t>;
	using BlendMap = std::map<std::int32_t, StringTableItem>;
	using TypeMap = std::map<std::int32_t, std::uint8_t>;

	class TintData
	{
	public:
		enum OverrideFlags
		{
			kNone = 0,
			kColor = (1 << 0),
			kTextureMap = (1 << 1),
			kBlendMap = (1 << 2),
			kTypeMap = (1 << 3)
		};

		bool empty() const { return m_textureMap.empty() && m_colorMap.empty() && m_blendMap.empty() && m_typeMap.empty(); }

		TextureMap m_textureMap;
		ColorMap m_colorMap;
		BlendMap m_blendMap;
		TypeMap m_typeMap;
	};

	void SetLayerColor(std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t color);
	void SetLayerType(std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t type);
	void SetLayerBlendMode(std::int32_t textureIndex, std::int32_t layerIndex, SKEEFixedString blendMode);
	void SetLayerTexture(std::int32_t textureIndex, std::int32_t layerIndex, SKEEFixedString texture);

	std::uint32_t GetLayerColor(std::int32_t textureIndex, std::int32_t layerIndex);
	std::uint32_t GetLayerType(std::int32_t textureIndex, std::int32_t layerIndex);
	SKEEFixedString GetLayerBlendMode(std::int32_t textureIndex, std::int32_t layerIndex);
	SKEEFixedString GetLayerTexture(std::int32_t textureIndex, std::int32_t layerIndex);

	void ClearLayerColor(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayerType(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayerBlendMode(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayerTexture(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayer(std::int32_t textureIndex);

	void SetData(SKEEFixedString key, SKEEFixedString value);
	SKEEFixedString GetData(SKEEFixedString key);
	bool HasData(SKEEFixedString key);
	void ClearData(SKEEFixedString key);

	void ForEachLayer(std::function<bool(std::int32_t, TintData&)> functor);
	bool GetLayer(std::int32_t layerIndex, std::function<void(TintData&)> functor);

private:
	std::mutex m_lock;
	std::unordered_map<std::int32_t, TintData> m_tintData;
	std::unordered_map<StringTableItem, StringTableItem> m_data;
};

typedef std::shared_ptr<ItemAttributeData> ItemAttributeDataPtr;

struct ShaderHasher
{
	std::size_t operator()(const RE::NiPointer<RE::BSLightingShaderProperty>& k) const
	{
		return (size_t)k.get();
	}
};

class CDXNifTextureRenderer
{
public:
	enum class TextureType : std::uint8_t
	{
		Normal = 0,
		Mask,
		Color,
		Unknown
	};
	struct MaskData
	{
		SKEEFixedString texture;
		SKEEFixedString technique = "normal";
		std::uint32_t color = 0xFFFFFF;
		TextureType textureType = TextureType::Mask;
	};

};
typedef std::unordered_map<RE::NiPointer<RE::BSLightingShaderProperty>, std::unordered_map<std::int32_t, std::shared_ptr<CDXNifTextureRenderer>>, ShaderHasher> TintMaskCacheMap;

class TintMaskMap : public SafeDataHolder<TintMaskCacheMap>
{
public:
	void ManageRenderTargetGroups();
	std::shared_ptr<CDXNifTextureRenderer> GetRenderTarget(RE::BSLightingShaderProperty* key, std::int32_t index);
	void AddRenderTargetGroup(RE::BSLightingShaderProperty* key, std::int32_t index, std::shared_ptr<CDXNifTextureRenderer> value);
	void ReleaseRenderTargetGroups();

	bool IsCaching() const { return m_caching; }

private:
	bool m_caching;
};

typedef std::unordered_map<std::int32_t, SKEEFixedString>	LayerTextureMap;
typedef std::unordered_map<std::int32_t, std::int32_t>			LayerColorMap;
typedef std::unordered_map<std::int32_t, float>			LayerAlphaMap;
typedef std::unordered_map<std::int32_t, SKEEFixedString>	LayerBlendMap;
typedef std::unordered_map<std::int32_t, std::uint8_t>			TextureTypeMap;
typedef std::unordered_multimap<std::int32_t, std::int32_t>		LayerSlotMap;

struct TextureLayer
{
	LayerTextureMap textures;
	LayerColorMap colors;
	LayerAlphaMap alphas;
	LayerBlendMap blendModes;
	TextureTypeMap types;
	LayerSlotMap slots;
};

// maps diffuse names to layer data
class TextureLayerMap : public std::unordered_map<SKEEFixedString, TextureLayer>
{
public:
	TextureLayer* GetTextureLayer(SKEEFixedString texture);
};

// maps trishape names to diffuse names
class MaskTriShapeMap : public std::unordered_map<SKEEFixedString, TextureLayerMap>
{
public:
	TextureLayerMap* GetTextureMap(SKEEFixedString triShape);

	bool IsRemappable() const { return m_remappable; }
	void SetRemappable(bool remap) { m_remappable = remap; }

protected:
	bool m_remappable;
};


typedef std::unordered_map<SKEEFixedString, MaskTriShapeMap> MaskModelContainer;

// Maps model names to trishape names
class MaskModelMap : public SafeDataHolder<MaskModelContainer>
{
public:
	TextureLayer* GetMask(SKEEFixedString nif, SKEEFixedString trishape, SKEEFixedString diffuse);

	SKEEFixedString GetModelPath(std::uint8_t gender, bool isFirstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* arma);
	void ApplyLayers(RE::TESObjectREFR* refr, bool isFirstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* arma, RE::NiAVObject* node, std::function<void(RE::NiPointer<RE::BSGeometry>, std::int32_t, TextureLayer*)> functor);
	MaskTriShapeMap* GetTriShapeMap(SKEEFixedString nifPath);
};

struct LayerTarget
{
	enum TargetFlags
	{
		kTarget_EmissiveColor = 1,
	};

	RE::NiPointer<RE::BSGeometry>			object;
	std::uint32_t					targetIndex;
	std::uint32_t					targetFlags;
	LayerTextureMap					textureData;
	LayerColorMap					colorData;
	LayerAlphaMap					alphaData;
	LayerBlendMap					blendData;
	TextureTypeMap					typeData;
	LayerSlotMap					slots;
};
typedef std::vector<LayerTarget> LayerTargetList;
typedef std::function<void(RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, RE::NiTexturePtr, LayerTarget&)> LayerFunctor;

class TintMaskInterface
	: public IPluginInterface
	, public IAddonAttachmentInterface
	, public RE::BSTEventSink<SKSE::NiNodeUpdateEvent>
{
public:
	virtual std::uint32_t GetVersion() = 0;

	enum ColorPreset
	{
		kPreset_Skin = -2,
		kPreset_Hair = -1
	};
	enum UpdateFlags
	{
		kUpdate_Skin = 1,
		kUpdate_Hair = 1 << 1,
		kUpdate_All = kUpdate_Skin | kUpdate_Hair
	};

	virtual void ApplyMasks(RE::TESObjectREFR* refr, bool isFirstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, std::uint32_t flags, ItemAttributeDataPtr overrides, LayerFunctor layer = LayerFunctor()) = 0;
	virtual void ManageTints() = 0;
	virtual void ReleaseTints() = 0;
	virtual void Revert() = 0;

	virtual bool IsDyeable(RE::TESObjectARMO* armor) = 0;

	virtual void GetTemplateColorMap(RE::TESObjectREFR* actor, RE::TESObjectARMO* armor, std::map<std::int32_t, std::uint32_t>& colorMap) = 0;
	virtual void GetSlotTextureIndexMap(RE::TESObjectREFR* actor, RE::TESObjectARMO* armor, std::map<std::int32_t, std::uint32_t>& slotTextureIndexMap) = 0;

	virtual void LoadMods() = 0;

protected:

private:
	virtual void OnAttach(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool isFirstPerson, RE::NiNode* skeleton, RE::NiNode* root) = 0;

	MaskModelMap	m_modelMap;
	TintMaskMap		m_maskMap;

	SimpleLock						m_dyeableLock;
	std::unordered_map<std::uint32_t, bool> m_dyeable;

	using EventResult = RE::BSEventNotifyControl;
	virtual EventResult ReceiveEvent(SKSE::NiNodeUpdateEvent* evn, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>* dispatcher) = 0;
};
