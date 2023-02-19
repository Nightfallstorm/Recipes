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
	static bool thunk(RE::TESDescription* a_description, std::uint64_t unk, std::uint64_t unk1, std::uint64_t unk2)
	{
		logger::info("RecipeReadInventoryHook triggered!");

		auto book = skyrim_cast<RE::TESObjectBOOK*, RE::TESDescription>(a_description);
		if (book && Recipe::isBookRecipe(book)) {
			logger::info("RecipeReadInventoryHook is recipe!");
			reinterpret_cast<Recipe::BookRecipe*>(book)->learnIngredients();
		}

		return func(a_description, unk, unk1, unk2);
	}	

	static inline REL::Relocation<decltype(thunk)> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(50991, 51870), REL::VariantOffset(0xE1, 0xD7, 0xE6) };
		stl::write_thunk_call<RecipeReadInventoryHook>(target.address());
		logger::info("RecipeReadInventoryHook hooked at address: {:x}!", target.address());
		logger::info("RecipeReadInventoryHook hooked at offset: {:x}!", target.offset());
	}
};

struct GetDescriptionHookSE
{
	static void thunk(RE::TESDescription* a_description, RE::BSString* a_out, RE::TESForm* a_parent, std::uint32_t unk)
	{
#ifdef _DEBUG
		logger::info("GetDescriptionHookSE triggered!");
#endif
		func(a_description, a_out, a_parent, unk);  // invoke original to get original description string output
		if (a_parent && a_parent->As<RE::TESObjectBOOK>() && Recipe::isBookRecipe(a_parent->As<RE::TESObjectBOOK>())) {
#ifdef _DEBUG
			logger::info("Correcting ingredients!");
#endif
			auto newText = Recipe::BookRecipe::correctIngredients(a_out);
			if (newText.size() > 0) {  // Simple size check in case of a bug
				*a_out = newText;
			} 
		}
	}

	static inline REL::Relocation<decltype(thunk)> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		assert(REL::Module::IsSE() || REL::Module::IsVR());
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(14399, 0), REL::VariantOffset(0x53, 0x0, 0x53) };
		stl::write_thunk_call<GetDescriptionHookSE>(target.address());
		logger::info("GetDescriptionHookSE hooked at address: {:x}!", target.address());
		logger::info("GetDescriptionHookSE hooked at offset: {:x}!", target.offset());
	}
};

// Track if a book is valid between the two AE hooks
static inline bool bookValidAE = false;
struct ValidateBookAE
{
	// AE inlines GetDescription, so we have to use a new hook right after the check for the parent form
	// so we can validate that parent form is a book recipe
	static void thunk(RE::BSString* a_out, RE::TESForm* a_parent, std::uint64_t a_unk)
	{
		func(a_out, nullptr, 0); // Invoke original (BSString::Set(a_out, 0, 0)
#ifdef _DEBUG
		logger::info("ValidateBookAE triggered!");
#endif
		if (a_parent && a_parent->As<RE::TESObjectBOOK>() && Recipe::isBookRecipe(a_parent->As<RE::TESObjectBOOK>())) {
			bookValidAE = true; // Set book valid for GetDescription hook to check
#ifdef _DEBUG
			logger::info("ValidateBookAE found valid recipe book!");
#endif
		}
	}

	static inline REL::Relocation<decltype(thunk)> func;

	struct GetParentInsideHook : Xbyak::CodeGenerator
	{
		// We can trust rdx will be scratched over when we jump back to the skyirm code
		// This moves the parent form, which is currently in rdi, to rdx so our ValidateBookAE hook can use it
		// rdx is already zeroed out, so we aren't worried about losing any data
		GetParentInsideHook()
		{
			mov(rdx, rdi);
		}
	};
	// Install our hook at the specified address
	static inline void Install()
	{
		assert(REL::Module::IsAE());
		REL::Relocation<std::uintptr_t> codeSwap{ RELOCATION_ID(0, 14552), REL::VariantOffset(0x0, 0x8B, 0x0) };
		REL::safe_fill(codeSwap.address(), REL::NOP, 0x5);  // Fill with NOP just in case
		auto newCode = GetParentInsideHook();
		assert(newCode.getSize() <= 0x5);
		REL::safe_write(codeSwap.address(), newCode.getCode(), newCode.getSize());

		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(0, 14552), REL::VariantOffset(0x0, 0x93, 0x0) };
		stl::write_thunk_call<ValidateBookAE>(target.address());

		logger::info("ValidateBookAE hooked at address: {:x}!", target.address());
		logger::info("ValidateBookAE hooked at offset: {:x}!", target.offset());
	}
};
struct GetDescriptionHookAE
{
	static void thunk(RE::BSString* a_out, std::uint64_t a_unk, std::uint64_t a_unk2)
	{
#ifdef _DEBUG
		logger::info("GetDescriptionHookAE triggered!");
#endif
		func(a_out, a_unk, a_unk2);  // invoke original

		if (bookValidAE) {
#ifdef _DEBUG
			logger::info("Correcting ingredients");
#endif
			auto newText = Recipe::BookRecipe::correctIngredients(a_out);
			if (newText.size() > 0) {  // Simple size check in case of a bug
				*a_out = newText;
			}
		}
		bookValidAE = false;  // Reset book valid status
	}

	static inline REL::Relocation<decltype(thunk)> func;

	// Install our hook at the specified address
	static inline void Install()
	{
		assert(REL::Module::IsAE());
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(0, 14552), REL::VariantOffset(0x0, 0xFB, 0x0) };
		REL::Relocation<std::uintptr_t> target2{ RELOCATION_ID(0, 14552), REL::VariantOffset(0x0, 0x174, 0x0) };
		stl::write_thunk_call<GetDescriptionHookAE>(target.address());
		stl::write_thunk_call<GetDescriptionHookAE>(target2.address());
		logger::info("GetDescriptionHookAE hooked at address: {:x}!", target.address());
		logger::info("GetDescriptionHookAE hooked at offset: {:x}!", target.offset());

		logger::info("GetDescriptionHookAE hooked at address: {:x}!", target2.address());
		logger::info("GetDescriptionHookAE hooked at offset: {:x}!", target2.offset());
	}
};
