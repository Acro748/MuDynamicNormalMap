#pragma once

namespace MDNM {
	class DynamicNormalMap :
		public IDynamicNormalMap
	{
	public:
		virtual std::uint32_t GetVersion() override { return 0; };
		
		virtual void QBakeObjectNormalmap(RE::Actor* a_actor, std::uint32_t bipedSlot) override {
			Mus::TaskManager::GetSingleton().QBakeObjectNormalMap(a_actor, Mus::TaskManager::GetSingleton().GetSkinGeometries(a_actor, bipedSlot), bipedSlot);
		}
		virtual void QBakeObjectNormalmap(RE::Actor* a_actor, RE::BSGeometry** a_geometries, std::uint32_t a_geometryCount, std::uint32_t bipedSlot) override {
			if (!a_geometries)
				return;
			std::unordered_set<RE::BSGeometry*> geos;
			for (std::uint32_t i = 0; i < a_geometryCount; i++) {
				if (!a_geometries[i])
					continue;
				geos.insert(a_geometries[i]);
			}
			Mus::TaskManager::GetSingleton().QBakeObjectNormalMap(a_actor, geos, bipedSlot);
		}
	};
	static DynamicNormalMap DNM;
}