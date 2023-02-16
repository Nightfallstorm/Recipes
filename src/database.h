#pragma once
#include "PCH.h"

class DataBase
{
public:
	std::vector<RE::IngredientItem*> ingredients;
	std::set<RE::EffectSetting*> effects;
	RE::BGSKeyword* vendorItemRecipeKeyword;
	RE::FormID skillIncreaseSound = 0x3C7CF; // UISkillIncreaseSD

	inline static DataBase*& GetSingleton()
	{
		static DataBase* _this_database = nullptr;
		if (!_this_database)
			_this_database = new DataBase();
		return _this_database;
	}

	/*
		@brief Safely de-allocate the memory space used by DataBase.
		*/
	static void Dealloc()
	{
		delete GetSingleton();
		GetSingleton() = nullptr;
	}

private:
	DataBase()
	{
		_initialize();
	}

	void _initialize()
	{
		auto const& [map, lock] = RE::TESForm::GetAllForms();
		vendorItemRecipeKeyword = RE::TESForm::LookupByID(0x000F5CB0)->As<RE::BGSKeyword>();
		lock.get().LockForRead();
		
		for (auto const& [formid, form] : *map) {
			if (form->Is(RE::IngredientItem::FORMTYPE)) {
				ingredients.push_back(form->As<RE::IngredientItem>());
			} else if (form->Is(RE::AlchemyItem::FORMTYPE)) {
				auto name = std::string(form->As<RE::AlchemyItem>()->GetFullName());
				std::transform(name.begin(), name.end(), name.begin(), ::toupper);
				if (name.find("POTION") != std::string::npos || name.find("POISON") != std::string::npos) {
					for (auto effect : form->As<RE::AlchemyItem>()->effects) {
						effects.insert(effect->baseEffect);
					}
				}
			}
		}

		lock.get().UnlockForRead();
	}
};
