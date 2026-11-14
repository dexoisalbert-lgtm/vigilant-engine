#pragma once

#include <string>
#include <string_view>

#include "PaleotrenchCore/Api.h"
#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/CaveGraph.h"

namespace pt
{
	// Reads one row of Data/CaveTuning.csv (the same file the game imports), so the cavegen tool and the
	// tests build exactly the caves the game builds. Columns that are not layout or rock settings (streaming,
	// lights) are ignored; missing columns keep their defaults. False with a reason if the row is missing or a
	// value is not a number.
	PT_API bool ReadCaveTuningRow(std::string_view CsvText, std::string_view Row, CaveLayoutTuning& OutLayout,
		CaveFieldTuning& OutField, std::string& OutError);

	// Row name for a boss cave (1-4), as used in the CSV.
	PT_API const char* CaveTuningRowName(int CaveId);
}
