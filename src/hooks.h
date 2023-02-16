#pragma once
#include "PCH.h"
#include "database.h"
#include "recipe.h"

struct RecipeReadWorldHook
{
	static bool thunk(RE::TESObjectBOOK* a_self, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3, RE::TESBoundObject* a_object, std::int32_t a_targetCount)
	{
		logger::info("RecipeReadWorldHook triggered!");
		if (Recipe::isBookRecipe(a_self) && a_activatorRef == RE::PlayerCharacter::GetSingleton()) {
			logger::info("RecipeReadWorldHook is recipe!");
			reinterpret_cast<Recipe::BookRecipe*>(a_self)->learnIngredients();
		}
		

		return func(a_self, a_targetRef, a_activatorRef, a_arg3, a_object, a_targetCount);
	}

	static inline REL::Relocation<decltype(thunk)> func;
	static inline std::uint32_t idx = 55;

	// Install our hook at the specified address
	static inline void Install()
	{
		stl::write_vfunc<RE::TESObjectBOOK, 0, RecipeReadWorldHook>(); // TESObjectBook::Activate

		logger::info("RecipeReadWorldHook hooked!");
	}
};

struct RecipeReadInventoryHook
{
	static bool thunk(std::uintptr_t a_description, std::uint64_t unk, std::uint64_t unk1, std::uint64_t unk2)
	{
		logger::info("RecipeReadInventoryHook triggered!");
		auto book = reinterpret_cast<RE::TESObjectBOOK*>(a_description - 0xA8);  // TODO: Cast this cleaner?
		if (Recipe::isBookRecipe(book)) {
			logger::info("RecipeReadInventoryHook is recipe!");
			reinterpret_cast<Recipe::BookRecipe*>(book)->learnIngredients();
		}

		return func(a_description, unk, unk1, unk2);
	}	

	static inline REL::Relocation<decltype(thunk)> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(50991, 51870), REL::VariantOffset(0xE1, 0xD7, 0xE6) };  // TODO: AE/VR
		stl::write_thunk_call<RecipeReadInventoryHook>(target.address());
		logger::info("RecipeReadInventoryHook hooked at address: {:x}!", target.address());
		logger::info("RecipeReadInventoryHook hooked at offset: {:x}!", target.offset());
	}
};

struct GetDescriptionHookSE
{
	static void thunk(RE::TESDescription* a_description, RE::BSString* a_out, RE::TESForm* a_parent, std::uint32_t unk)
	{
		logger::info("GetDescriptionHook triggered!");
		func(a_description, a_out, a_parent, unk);  // invoke original to get original description string output
		if (a_parent && a_parent->As<RE::TESObjectBOOK>() && Recipe::isBookRecipe(a_parent->As<RE::TESObjectBOOK>())) {
			logger::info("GetDescriptionHook is recipe!");
			*a_out = reinterpret_cast<Recipe::BookRecipe*>(a_parent->As<RE::TESObjectBOOK>())->correctIngredients(a_out);
		}
	}

	static inline REL::Relocation<decltype(thunk)> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(14399, 0), REL::VariantOffset(0x53, 0x0, 0x53) };  // TODO: AE/VR
		stl::write_thunk_call<GetDescriptionHookSE>(target.address());
		logger::info("GetDescription hooked at address: {:x}!", target.address());
		logger::info("GetDescription hooked at offset: {:x}!", target.offset());
	}
};
