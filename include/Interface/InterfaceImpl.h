#pragma once

namespace MDNM {
	class DynamicNormalMap :
		public IDynamicNormalMap
	{
	public:
		virtual std::uint32_t GetVersion() override { return 0; };
		
		virtual void QUpdateNormalmap(RE::Actor* a_actor, std::uint32_t a_updateBipedSlot) override {
			Mus::TaskManager::GetSingleton().QUpdateNormalMap(a_actor, Mus::TaskManager::GetSingleton().GetGeometries(a_actor, a_updateBipedSlot), a_updateBipedSlot);
		}
		virtual void QUpdateNormalmap(RE::Actor* a_actor, RE::BSGeometry** a_geometries, std::uint32_t a_geometryCount, std::uint32_t a_updateBipedSlot) override {
			if (!a_geometries)
				return;
			std::unordered_set<RE::BSGeometry*> geos;
			for (std::uint32_t i = 0; i < a_geometryCount; i++) {
				if (!a_geometries[i])
					continue;
				geos.insert(a_geometries[i]);
			}
			Mus::TaskManager::GetSingleton().QUpdateNormalMap(a_actor, geos, a_updateBipedSlot);
		}
		virtual void QUpdateNormalmap(RE::Actor* a_actor, RE::BSGeometry** a_geometries, std::uint32_t a_geometryCount, const char** a_updateGeometryNames, std::uint32_t a_updateGeometryCount) {
			if (!a_geometries || !a_updateGeometryNames)
				return;
			std::unordered_set<RE::BSGeometry*> geos;
			for (std::uint32_t i = 0; i < a_geometryCount; i++) {
				if (!a_geometries[i])
					continue;
				geos.insert(a_geometries[i]);
			}
			std::unordered_set<std::string> updateTargets;
			for (std::uint32_t i = 0; i < a_updateGeometryCount; i++) {
				if (!a_updateGeometryNames[i])
					continue;
				updateTargets.insert(std::string(a_updateGeometryNames[i]));
			}
		}
		virtual void QUpdateNormalmap(RE::Actor* a_actor, RE::BSGeometry** a_geometries, std::uint32_t a_geometryCount, RE::BSGeometry** a_updateGeometries, std::uint32_t a_updateGeometryCount) {
			if (!a_geometries || !a_updateGeometries)
				return;
			std::unordered_set<RE::BSGeometry*> geos;
			for (std::uint32_t i = 0; i < a_geometryCount; i++) {
				if (!a_geometries[i])
					continue;
				geos.insert(a_geometries[i]);
			}
			std::unordered_set<std::string> updateTargets;
			for (std::uint32_t i = 0; i < a_updateGeometryCount; i++) {
				if (!a_updateGeometries[i])
					continue;
				updateTargets.insert(std::string(a_updateGeometries[i]->name.c_str()));
			}
		}
	};
	static DynamicNormalMap DNM;
}