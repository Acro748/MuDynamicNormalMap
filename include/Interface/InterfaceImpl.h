#pragma once

namespace MDNM {
	class DynamicNormalMap :
		public IDynamicNormalMap
	{
	public:
		virtual std::uint32_t GetVersion() override { return 0; };
		
		virtual void QUpdateNormalmap(RE::Actor* a_actor, std::uint32_t a_updateBipedSlot) override {
			Mus::TaskManager::GetSingleton().QUpdateNormalMap(a_actor, Mus::TaskManager::GetSingleton().GetAllGeometries(a_actor), a_updateBipedSlot);
		}
		virtual void QUpdateNormalmap(RE::Actor* a_actor, RE::BSGeometry** a_geometries, std::uint32_t a_geometryCount) {
			if (!a_actor || !a_actor->loadedData || !a_actor->loadedData->data3D)
				return;
			if (!a_geometries)
				return;
			std::unordered_set<RE::BSGeometry*> geos;
			for (std::uint32_t i = 0; i < a_geometryCount; i++) {
				if (!a_geometries[i])
					continue;
				geos.insert(a_geometries[i]);
			}
			Mus::TaskManager::GetSingleton().QUpdateNormalMap(a_actor, Mus::TaskManager::GetSingleton().GetAllGeometries(a_actor), geos);
		}
		virtual void SetDetailStrength(RE::Actor* a_actor, float a_strength) {
			if (!a_actor)
				return;
			Mus::Papyrus::detailStrengthMap[a_actor->formID] = std::clamp(a_strength, 0.0f, 1.0f);
		}
	};
	static DynamicNormalMap DNM;
}