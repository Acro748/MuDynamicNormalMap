#include "Condition.h"

namespace Mus {
	void ConditionManager::InitialConditionMap()
	{
		ConditionMap.clear();
		auto types = magic_enum::enum_entries<ConditionType>();
		for (auto& type : types)
		{
			ConditionMap.emplace(lowLetter(type.second.data()), type.first);
		}
	}

	bool ConditionManager::ConditionCheck(RE::Actor* a_actor, Condition condition)
	{
		if (!std::ranges::all_of(condition.AND, [&](auto& AND)
			{
				return std::ranges::any_of(AND, [&](auto& OR)
					{
						bool isTrue = OR.NOT ? !OR.conditionFunction->Condition(a_actor) : OR.conditionFunction->Condition(a_actor);
						if (Config::GetSingleton().GetLogLevel() < 2)
							Logging(a_actor, OR, isTrue);
						return isTrue;
					}
				);
			}
		))
			return false;
		return true;
	}

	const ConditionManager::Condition ConditionManager::GetCondition(RE::Actor* a_actor)
	{
		for (auto& condition : ConditionList)
		{
			if (ConditionCheck(a_actor, condition))
			{
				return condition;
			}
		}
		return Condition();
	}

	bool ConditionManager::RegisterCondition(Condition condition)
	{
		ConditionList.push_back(ParseConditions(condition));
		return true;
	}

	void ConditionManager::SortConditions()
	{
		std::sort(ConditionList.begin(), ConditionList.end(), [](Condition& a, Condition& b) {
			return a.Priority > b.Priority;
		});
	}

	const ConditionManager::Condition ConditionManager::ParseConditions(Condition condition)
	{
		std::vector<std::string> splittedANDs = split(condition.originalCondition, "AND");

		bool firstAND = true;
		for (auto& strAnd : splittedANDs)
		{
			if (!firstAND)
				logger::debug("AND ...");
			firstAND = false;
			std::vector<std::string> splittedORs = split(strAnd, "OR");
			ConditionItemOr conditionOr;

			bool firstOR = true;
			for (auto& strOr : splittedORs)
			{
				ConditionItem Item;
				if (stringStartsWith(strOr, "NOT"))
				{
					Item.NOT = true;
					strOr.erase(0, 3);
					trim(strOr);
				}

				bool invalid = false;
				if (GetConditionType(strOr, Item) == ConditionType::Error)
					invalid = true;
				else
				{
					if (GetConditionFunction(Item))
					{
						conditionOr.emplace_back(Item);
						logger::debug("{}{}{} ...", firstOR ? "" : "OR ", Item.NOT ? "NOT " : "", magic_enum::enum_name(Item.type).data());
					}
					else
						invalid = false;
				}

				if (invalid)
				{
					logger::error("Found invalid condition! : {}{}{}", firstOR ? "" : "OR ", Item.NOT ? "NOT " : "", strOr);
					logger::error("Cause file : {}", condition.fileName);
				}
				firstOR = false;
			}
			condition.AND.emplace_back(conditionOr);
		}
		return condition;
	}

	const ConditionManager::ConditionType ConditionManager::GetConditionType(std::string line, ConditionItem& item)
	{
		std::vector<std::string> splittedMain = splitMulti(line, "()");
		if (splittedMain.size() == 0)
		{
			item.type = ConditionType::None;
			return ConditionType::None;
		}
		std::string low = lowLetter(splittedMain[0]);
		if (auto found = ConditionMap.find(low); found != ConditionMap.end())
			item.type = found->second;
		else
		{
			item.type = ConditionType::Error;
			return ConditionType::Error;
		}

		if (splittedMain.size() > 1)
		{
			std::vector<std::string> splitted = split(splittedMain[1], "|");
			if (splitted.size() == 1)
			{
				item.id = GetHex(splitted[0]);
				item.arg = splitted[0];
			}
			else if (splitted.size() == 2)
			{
				item.pluginName = splitted[0];
				item.id = GetHex(splitted[1]);
				item.arg = splitted[0];
			}
			else if (splitted.size() == 3)
			{
				item.pluginName = splitted[0];
				item.id = GetHex(splitted[1]);
				item.arg = splitted[2];
			}
		}
		return item.type;
	}

	bool ConditionManager::GetConditionFunction(ConditionItem& item)
	{
		switch (item.type) {
		case ConditionType::HasKeyword:
			item.conditionFunction = std::make_shared<ConditionFragment::HasKeyword>();
			break;
		case ConditionType::HasKeywordEditorID:
			item.conditionFunction = std::make_shared<ConditionFragment::HasKeywordEditorID>();
			break;
		case ConditionType::IsActorBase:
			item.conditionFunction = std::make_shared<ConditionFragment::IsActorBase>();
			break;
		case ConditionType::IsActor:
			item.conditionFunction = std::make_shared<ConditionFragment::IsActor>();
			break;
		case ConditionType::IsRace:
			item.conditionFunction = std::make_shared<ConditionFragment::IsRace>();
			break;
		case ConditionType::IsFemale:
			item.conditionFunction = std::make_shared<ConditionFragment::IsFemale>();
			break;
		case ConditionType::IsChild:
			item.conditionFunction = std::make_shared<ConditionFragment::IsChild>();
			break;
		case ConditionType::None:
			item.conditionFunction = std::make_shared<ConditionFragment::NoneCondition>();
			break;
		}
		if (!item.conditionFunction)
			return false;
		item.conditionFunction->Initial(item);
		return true;
	}

	static RE::NiStream* NiStream_ctor(RE::NiStream* stream) {
		using func_t = decltype(&NiStream_ctor);
		REL::VariantID offset(68971, 70324, 0x00C9EC40);
		REL::Relocation<func_t> func{ offset };
		return func(stream);
	}
	static void NiStream_dtor(RE::NiStream* stream) {
		using func_t = decltype(&NiStream_dtor);
		REL::VariantID offset(68972, 70325, 0x00C9EEA0);
		REL::Relocation<func_t> func{ offset };
		return func(stream);
	}
	namespace ConditionFragment
	{
		void HasKeyword::Initial(ConditionManager::ConditionItem& item)
		{
			keyword = GetFormByID<RE::BGSKeyword*>(item.id, item.pluginName);
		}
		bool HasKeyword::Condition(RE::Actor* actor)
		{
			if (!keyword)
				return false;
			RE::TESRace* race = actor->GetRace();
			return (actor->HasKeyword(keyword) || (race ? race->HasKeyword(keyword) : false));
		}

		void HasKeywordEditorID::Initial(ConditionManager::ConditionItem& item)
		{
			keywordEditorID = item.arg;
		}
		bool HasKeywordEditorID::Condition(RE::Actor* actor)
		{
			return actor ? actor->HasKeywordString(keywordEditorID.c_str()) : false;
		}

		void IsActorBase::Initial(ConditionManager::ConditionItem& item)
		{
			form = GetFormByID(item.id, item.pluginName);
		}
		bool IsActorBase::Condition(RE::Actor* actor)
		{
			if (!actor || !form)
				return false;
			RE::TESNPC* actorBase = actor->GetActorBase();
			return actorBase && actorBase->formID == form->formID;
		}

		void IsActor::Initial(ConditionManager::ConditionItem& item)
		{
			form = GetFormByID(item.id, item.pluginName);
		}
		bool IsActor::Condition(RE::Actor* actor)
		{
			if (!actor || !form)
				return false;
			return actor->formID == form->formID;
		}

		void IsRace::Initial(ConditionManager::ConditionItem& item)
		{
			form = GetFormByID(item.id, item.pluginName);
		}
		bool IsRace::Condition(RE::Actor* actor)
		{
			if (!actor || !form)
				return false;
			RE::TESRace* race = actor->GetRace();
			return race && race->formID == form->formID;
		}

		void IsFemale::Initial(ConditionManager::ConditionItem& item)
		{
		}
		bool IsFemale::Condition(RE::Actor* actor)
		{
			if (!actor)
				return false;
			RE::TESNPC* actorBase = actor->GetActorBase();
			return actorBase && actorBase->GetSex() == RE::SEX::kFemale;
		}

		void IsChild::Initial(ConditionManager::ConditionItem& item)
		{
		}
		bool IsChild::Condition(RE::Actor* actor)
		{
			if (!actor)
				return false;
			return actor->IsChild();
		}

		void NoneCondition::Initial(ConditionManager::ConditionItem& item)
		{
		}
		bool NoneCondition::Condition(RE::Actor* actor)
		{
			return true;
		}
	}
}
