#pragma once
#include "PCH.h"
#include "database.h"
#include <rapidfuzz/fuzz.hpp>

namespace Recipe
{
	static void learnEffect(RE::IngredientItem* a_ingredient, std::uint32_t a_aiIndex)
	{
		// TODO: This is in PO3 CLIB, grab from CLIB-NG once updated
		a_ingredient->gamedata.knownEffectFlags |= 1 << a_aiIndex;
		a_ingredient->AddChange(0x80000000);
	}

	static bool hasLearnedEffect(RE::IngredientItem* a_ingredient, std::uint32_t a_aiIndex)
	{
		// TODO: This is in PO3 CLIB, grab from CLIB-NG once updated
		return (a_ingredient->gamedata.knownEffectFlags & (1 << a_aiIndex)) != 0;
	}

	static bool isBookRecipe(RE::TESObjectBOOK* a_book)
	{
		return a_book->HasKeyword(DataBase::GetSingleton()->vendorItemRecipeKeyword);
	}
	class BookRecipe : RE::TESObjectBOOK
	{
	public:
		std::string getRecipeText()
		{
			RE::BSString description;
			this->GetDescription(description, this);
			return std::string(description.c_str());
		}

		std::vector<RE::IngredientItem*> getIngredients()
		{
			std::vector<RE::IngredientItem*> ingredients;
			for (auto line : getRecipeLines()) {
				if (line.find("~") != std::string::npos) {
					RE::IngredientItem* bestMatchingIngredient = nullptr;
					for (auto ingredient : DataBase::GetSingleton()->ingredients) {
						if (stl::string::icontains(line, ingredient->GetFullName())) {
							if (!bestMatchingIngredient || bestMatchingIngredient->GetFullNameLength() < ingredient->GetFullNameLength()) {
								// Some ingredients like Watcher's Eye match both Watcher's eye and Blind Watcher's Eye, so get the best match
								logger::info("Initially matched {}", ingredient->GetFullName());
								bestMatchingIngredient = ingredient;
							}
						}
					}

					if (bestMatchingIngredient) {
						logger::info("ingredient found as {}:{:x}!", bestMatchingIngredient->GetFullName(), bestMatchingIngredient->formID);
						ingredients.push_back(bestMatchingIngredient);
					} else {
						logger::warn("Couldn't parse {} for ingredient", line);
					}
				}
			}

			return ingredients;
		}

		std::string getRecipeEffectName()
		{
			std::string recipeEffectName;
			for (auto line : getRecipeLines()) {
				if (stl::string::icontains(line, "potion") || stl::string::icontains(line, "poison")) {
					// Note: We are intentionally NOT finding a matching effect because multiple effects can have the same name.
					// Therefore, we will just check each ingredient's effect's name against the name in the recipe to confirm matches
					recipeEffectName = line;
					logger::info("Recipe effect to check against is now {}", recipeEffectName);
				}
			}

			return recipeEffectName;
		}

		std::vector<std::string> getRecipeLines()
		{
			return stl::splitLines(getRecipeText());
		}

		void learnIngredients()
		{
			auto recipeEffect = getRecipeEffectName();
			if (recipeEffect != "") {
				// learn ingredient effects
				bool learnedEffect = false;
				for (auto ingredient : getIngredients()) {
					logger::info("Parsing ingredient: {}", ingredient->GetFullName());
					std::uint32_t index = 0;
					
					for (auto effect : ingredient->effects) {
						logger::info("Check effect {}, against recipeEffect", effect->baseEffect->GetFullName());
						if (recipeEffect.find(effect->baseEffect->GetFullName()) != std::string::npos && effect->baseEffect->GetFullNameLength() != 0) {
							if (!hasLearnedEffect(ingredient, index)) {
								learnEffect(ingredient, index);
								RE::DebugNotification(std::format("Discovered {} in {}", effect->baseEffect->GetFullName(), ingredient->GetFullName()).c_str());
								learnedEffect = true;
							}
						}

						index++;
					}					
				}

				if (learnedEffect) {
					RE::BSAudioManager::GetSingleton()->Play(DataBase::GetSingleton()->skillIncreaseSound);
				}
			}
		}

		static RE::BSString correctIngredients(RE::BSString* a_out)
		{
			std::string recipeEffectName;
			auto lines = stl::splitLines(a_out->c_str());
			std::vector<std::string> newLines;
			for (auto line : lines) {
				if (stl::string::icontains(line, "potion") || stl::string::icontains(line, "poison")) {
					// Note: We are intentionally NOT finding a matching effect because multiple effects can have the same name.
					// Therefore, we will just check each ingredient's effect's name against the name in the recipe to confirm matches
					recipeEffectName = line;
					logger::info("Recipe effect to check against for correcting ingredients is {}", recipeEffectName);
					break;
				}
			}

			std::vector<RE::IngredientItem*> ingredients;
			for (auto line : lines) {
				auto correctedLine = line;
				if (line.find("~") != std::string::npos) {
					RE::IngredientItem* matchedIngredient = nullptr;
					for (auto ingredient : DataBase::GetSingleton()->ingredients) {
						if (stl::string::icontains(line, ingredient->GetFullName())) {
							bool matchedEffect = false;
							for (auto effect : ingredient->effects) {
								if (effect && effect->baseEffect && stl::string::icontains(recipeEffectName, effect->baseEffect->GetFullName())) {
									matchedEffect = true;
									break;
								}
							}
							if (matchedEffect) {
								logger::info("Matched ingredient: {}", ingredient->GetFullName());
								matchedIngredient = ingredient;
							}
							break;
						}
					}

					if (!matchedIngredient) {
						auto closestIngredient = getClosestMatchingIngredient(line, recipeEffectName);
						if (closestIngredient && closestIngredient->GetFullName()) {
							correctedLine = std::format("~{}", closestIngredient->GetFullName());
							logger::info("Corrected ingredient {} to ~{}", line, closestIngredient->GetFullName());
						} else {
							logger::info("Could not correct ingredient {}", line);
						}
					}
				}
				newLines.push_back(correctedLine);
			}
			return RE::BSString(stl::combineLines(newLines));
		}

		static RE::IngredientItem* getClosestMatchingIngredient(std::string ingredientToCorrect, std::string recipeEffectName)
		{
			std::pair<RE::IngredientItem*, double> bestIngredientMatch = { nullptr, 0 };
			for (auto ingredient : DataBase::GetSingleton()->ingredients) {
				bool matchesRecipe = false;
				for (auto effect : ingredient->effects) {
					if (stl::string::icontains(recipeEffectName, effect->baseEffect->GetFullName())) {
						matchesRecipe = true;
						break;
					}
				}
				if (matchesRecipe) {
					double score = rapidfuzz::fuzz::token_sort_ratio(ingredient->GetFullName(), ingredientToCorrect);
					if (!bestIngredientMatch.first || bestIngredientMatch.second < score) {
						bestIngredientMatch = { ingredient, score };
					}
				}
			}

			return bestIngredientMatch.first;
		}
	};
}
