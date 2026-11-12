#pragma once

#include <cstdint>
#include <string_view>

#include "PaleotrenchCore/Api.h"

namespace pt
{
	// The hub and the four boss caves. Values are stored in save files: never
	// reorder or reuse them.
	enum class ECave : uint8_t
	{
		Hub = 0,
		Livyatan = 1,      // The Drowned Cathedral
		Tusoteuthis = 2,   // The Ink Galleries
		Stygiomedusa = 3,  // The Black Veil
		Arthropleura = 4,  // The Breathing Tunnels
	};

	inline constexpr int CaveCount = 5;

	PT_API const char* CaveName(ECave Cave);

	// Stable 64-bit hash of text (FNV-1a + mix). Lets players type a seed as words.
	PT_API uint64_t HashText(std::string_view Text);

	// Combines two values into a well-mixed 64-bit value.
	PT_API uint64_t MixSeed(uint64_t A, uint64_t B);

	// Every save file has one seed; each cave derives its own from it, so a
	// cave's layout is fixed for the lifetime of that save.
	PT_API uint64_t DeriveCaveSeed(uint64_t SaveSeed, ECave Cave);
}
