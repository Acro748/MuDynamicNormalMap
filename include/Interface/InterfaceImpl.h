#pragma once

namespace MDNM {
	class DynamicNormalMap :
		public IDynamicNormalMap
	{
	public:
		virtual std::uint32_t GetVersion() override { return 0; };
		
		virtual void QUpdateNormalmap(RE::Actor* a_actor, std::uint32_t a_updateBipedSlot) override {
			Mus::TaskManager::GetSingleton().QUpdateNormalMap(a_actor, a_updateBipedSlot);
		}
		virtual void SetDetailStrength(RE::Actor* a_actor, float a_strength) {
			if (!a_actor)
				return;
			Mus::Papyrus::detailStrengthMap[a_actor->formID] = std::clamp(a_strength, 0.0f, 1.0f);
		}
	};
	static DynamicNormalMap DNM;
}