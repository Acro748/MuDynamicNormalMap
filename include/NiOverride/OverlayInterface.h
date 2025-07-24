#pragma once

#include "IPluginInterface.h"

#include <set>
#include <string>

class OverlayHolder : public std::set<std::uint64_t>
{
public:
};

class OverlayInterface
	: public IPluginInterface
	, public IAddonAttachmentInterface
{
public:
	virtual std::uint32_t GetVersion() = 0;

	virtual void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion) = 0;
	virtual bool Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion) = 0;
	virtual void Revert() = 0;

	virtual bool HasOverlays(RE::TESObjectREFR* reference) = 0;
	virtual void AddOverlays(RE::TESObjectREFR* reference) = 0;
	virtual void RemoveOverlays(RE::TESObjectREFR* reference) = 0;
	virtual void RevertOverlays(RE::TESObjectREFR* reference, bool resetDiffuse) = 0;
	virtual void RevertOverlay(RE::TESObjectREFR* reference, RE::BSFixedString nodeName, std::uint32_t armorMask, std::uint32_t addonMask, bool resetDiffuse) = 0;

	virtual void EraseOverlays(RE::TESObjectREFR* reference) = 0;

	virtual void RevertHeadOverlays(RE::TESObjectREFR* reference, bool resetDiffuse) = 0;
	virtual void RevertHeadOverlay(RE::TESObjectREFR* reference, RE::BSFixedString nodeName, std::uint32_t partType, std::uint32_t shaderType, bool resetDiffuse) = 0;

	virtual void SetupOverlay(std::uint32_t primaryCount, const char* primaryPath, const char* primaryNode, std::uint32_t secondaryCount, const char* secondaryPath, const char* secondaryNode, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) = 0;

	virtual void UninstallOverlay(const char* nodeName, RE::TESObjectREFR* refr, RE::NiNode* parent) = 0;
	virtual void InstallOverlay(const char* nodeName, const char* path, RE::TESObjectREFR* refr, RE::BSGeometry* source, RE::NiNode* destination, RE::BGSTextureSet* textureSet = NULL) = 0;
	virtual void ResetOverlay(const char* nodeName, RE::TESObjectREFR* refr, RE::BSGeometry* source, RE::NiNode* destination, RE::BGSTextureSet* textureSet = NULL, bool resetDiffuse = false) = 0; // Re-applies the skin's textures

	virtual std::string& GetDefaultTexture() = 0;
	virtual void SetDefaultTexture(const std::string& newTexture) = 0;

	// Relinks an overlay node by name to the new source
	virtual void RelinkOverlay(const char* nodeName, RE::TESObjectREFR* refr, RE::BSGeometry* source, RE::NiNode* skeleton) = 0;

	// Relinks structured name of overlays to new source
	virtual void RelinkOverlays(std::uint32_t primaryCount, const char* primaryNode, std::uint32_t secondaryCount, const char* secondaryNode, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) = 0;

	// Builds default overlays
	virtual void BuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) = 0;

	// Relinks default overlays
	virtual void RebuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR* refr, RE::NiNode* boneTree, RE::NiAVObject* resultNode) = 0;

private:
	std::string defaultTexture;
	OverlayHolder overlays;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool isFirstPerson, RE::NiNode* skeleton, RE::NiNode* root) override = 0;
};
