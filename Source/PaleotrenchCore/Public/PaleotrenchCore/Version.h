#pragma once

#include "PaleotrenchCore/Api.h"

namespace pt
{
	// Version of the engine-free core. Bump when save data or generation
	// results change, so old saves can be detected.
	PT_API const char* CoreVersion();

	// Runs a handful of known-answer checks (random stream, seed derivation).
	// Called once at game start so a compiler or platform difference that would
	// change generated caves is caught immediately.
	PT_API bool RunSelfCheck();
}
