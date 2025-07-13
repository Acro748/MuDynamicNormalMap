#pragma once

namespace Mus {
	namespace ConditionFragment
	{
		class ConditionBase;
	}
	class ConditionManager {
	public :
		ConditionManager() {};
		~ConditionManager() {};

		[[nodiscard]] static ConditionManager& GetSingleton() {
			static ConditionManager instance;
			return instance;
		}

		void InitialConditionMap();

		enum ConditionType : std::uint32_t {
			HasKeyword,
			HasKeywordEditorID,
			IsActorBase,
			IsActor,
			IsRace,

			IsFemale,
			IsChild,

			None,
			Error
		};

		struct ConditionItem {
			std::shared_ptr<ConditionFragment::ConditionBase> conditionFunction;

			bool NOT = false;
			ConditionType type;
			std::string pluginName = "";
			RE::FormID id = 0;
			std::string arg = "";
		};
		typedef std::vector<ConditionItem> ConditionItemOr;

		struct Condition {
			std::string fileName;
			std::string originalCondition;
			std::vector<ConditionItemOr> AND;
			bool Enable = true;
			bool HeadEnable = true;
			std::vector<std::string> ProxyTangentTextureFolder;
			std::vector<std::string> ProxyOverlayTextureFolder;
			std::int32_t Priority;
		};
		bool RegisterCondition(Condition condition);
		void SortConditions();

		const Condition GetCondition(RE::Actor* a_actor);

		std::size_t ConditionCount() const { return ConditionList.size(); }
	private:
		concurrency::concurrent_vector<Condition> ConditionList;
		std::unordered_map<std::string, ConditionType> ConditionMap;

		const Condition ParseConditions(Condition condition);
		const ConditionType GetConditionType(std::string line, ConditionItem& item);

		bool GetConditionFunction(ConditionItem& item);
		bool ConditionCheck(RE::Actor* a_actor, Condition condition);

		inline void Logging(RE::Actor* a_actor, const ConditionItem& OR, bool isTrue) {
			std::string typestr = magic_enum::enum_name(ConditionType(OR.type)).data();
			if (IsContainString(typestr, "EditorID")
				|| IsContainString(typestr, "Type"))
			{
				logger::debug("{} {:x} : Condition {}{}({}) is {}", a_actor->GetName(), a_actor->formID,
							  OR.NOT ? "NOT " : "", typestr, OR.arg,
							  isTrue ? "True" : "False");
			}
			else if (OR.type >= ConditionType::IsFemale)
			{
				logger::debug("{} {:x} : Condition {}{}() is {}", a_actor->GetName(), a_actor->formID,
							  OR.NOT ? "NOT " : "", typestr,
							  isTrue ? "True" : "False");
			}
			else
			{
				logger::debug("{} {:x} : Condition {}{}({}{}{:x}) is {}", a_actor->GetName(), a_actor->formID,
							  OR.NOT ? "NOT " : "", typestr, OR.pluginName, OR.pluginName.empty() ? "" : "|", OR.id,
							  isTrue ? "True" : "False");
			}
		}
	};

	namespace ConditionFragment
	{
		class ConditionBase {
		public:
			virtual ~ConditionBase() = default;

			virtual void Initial(ConditionManager::ConditionItem& item) = 0;
			virtual bool Condition(RE::Actor* actor) = 0;
		protected:
			bool isLeft = true;
		};

		class HasKeyword : public ConditionBase {
		public:
			HasKeyword() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* actor) override;
		private:
			RE::BGSKeyword* keyword = nullptr;
		};

		class HasKeywordEditorID : public ConditionBase {
		public:
			HasKeywordEditorID() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* actor) override;
		private:
			std::string keywordEditorID = "";
		};

		class IsActorBase : public ConditionBase {
		public:
			IsActorBase() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* actor) override;
		private:
			RE::TESForm* form = nullptr;
		};

		class IsActor : public ConditionBase {
		public:
			IsActor() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* actor) override;
		private:
			RE::TESForm* form = nullptr;
		};

		class IsRace : public ConditionBase {
		public:
			IsRace() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* acto) override;
		private:
			RE::TESForm* form = nullptr;
		};

		class IsFemale : public ConditionBase {
		public:
			IsFemale() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* acto) override;
		};

		class IsChild : public ConditionBase {
		public:
			IsChild() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* acto) override;
		};

		class NoneCondition : public ConditionBase {
		public:
			NoneCondition() = default;
			void Initial(ConditionManager::ConditionItem& item) override;
			bool Condition(RE::Actor* acto) override;
		};
	}
}
