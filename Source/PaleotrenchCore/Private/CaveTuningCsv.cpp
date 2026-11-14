#include "PaleotrenchCore/CaveTuningCsv.h"

#include <cstdlib>
#include <vector>

namespace pt
{
	namespace
	{
		std::vector<std::string> SplitLine(std::string_view Line)
		{
			std::vector<std::string> Cells;
			std::string Cell;
			for (const char Ch : Line)
			{
				if (Ch == ',')
				{
					Cells.push_back(Cell);
					Cell.clear();
				}
				else if (Ch != '\r' && Ch != '"')
				{
					Cell.push_back(Ch);
				}
			}
			Cells.push_back(Cell);
			return Cells;
		}

		bool ToNumber(const std::string& Text, double& Out)
		{
			if (Text.empty())
			{
				return false;
			}
			char* End = nullptr;
			Out = std::strtod(Text.c_str(), &End);
			return End && *End == '\0';
		}
	}

	const char* CaveTuningRowName(int CaveId)
	{
		switch (CaveId)
		{
		case 1: return "Livyatan";
		case 2: return "Tusoteuthis";
		case 3: return "Stygiomedusa";
		case 4: return "Arthropleura";
		default: return "Livyatan";
		}
	}

	bool ReadCaveTuningRow(std::string_view CsvText, std::string_view Row, CaveLayoutTuning& OutLayout, CaveFieldTuning& OutField,
		std::string& OutError)
	{
		std::vector<std::vector<std::string>> Lines;
		size_t Start = 0;
		while (Start < CsvText.size())
		{
			size_t End = CsvText.find('\n', Start);
			if (End == std::string_view::npos)
			{
				End = CsvText.size();
			}
			const std::string_view Line = CsvText.substr(Start, End - Start);
			if (!Line.empty() && Line != "\r")
			{
				Lines.push_back(SplitLine(Line));
			}
			Start = End + 1;
		}
		if (Lines.empty())
		{
			OutError = "the file is empty";
			return false;
		}
		const std::vector<std::string>& Header = Lines.front();
		for (size_t r = 1; r < Lines.size(); ++r)
		{
			const std::vector<std::string>& Cells = Lines[r];
			if (Cells.empty() || Cells[0] != Row)
			{
				continue;
			}
			for (size_t c = 1; c < Header.size() && c < Cells.size(); ++c)
			{
				const std::string& Name = Header[c];
				double V = 0.0;
				if (!ToNumber(Cells[c], V))
				{
					OutError = "row " + std::string(Row) + ", column " + Name + ": '" + Cells[c] + "' is not a number";
					return false;
				}
				// Layout
				if (Name == "MinChambers") OutLayout.MinChambers = static_cast<int>(V);
				else if (Name == "MaxChambers") OutLayout.MaxChambers = static_cast<int>(V);
				else if (Name == "RegionLength") OutLayout.RegionLength = V;
				else if (Name == "RegionWidth") OutLayout.RegionWidth = V;
				else if (Name == "RegionHeight") OutLayout.RegionHeight = V;
				else if (Name == "ChamberRadiusMin") OutLayout.ChamberRadiusMin = V;
				else if (Name == "ChamberRadiusMax") OutLayout.ChamberRadiusMax = V;
				else if (Name == "ArenaRadius") OutLayout.ArenaRadius = V;
				else if (Name == "ChamberGap") OutLayout.ChamberGap = V;
				else if (Name == "TunnelRadiusMin") OutLayout.TunnelRadiusMin = V;
				else if (Name == "TunnelRadiusMax") OutLayout.TunnelRadiusMax = V;
				else if (Name == "SqueezeRadius") OutLayout.SqueezeRadius = V;
				else if (Name == "SqueezeChance") OutLayout.SqueezeChance = V;
				else if (Name == "ShaftSlope") OutLayout.ShaftSlope = V;
				else if (Name == "MaxPassageLength") OutLayout.MaxPassageLength = V;
				else if (Name == "CandidateNeighbours") OutLayout.CandidateNeighbours = static_cast<int>(V);
				else if (Name == "LoopFraction") OutLayout.LoopFraction = V;
				else if (Name == "MaxAirSwim") OutLayout.MaxAirSwim = V;
				else if (Name == "SideRoomChance") OutLayout.SideRoomChance = V;
				// Rock
				else if (Name == "BlendRadius") OutField.BlendRadius = V;
				else if (Name == "LumpStrength") OutField.LumpStrength = V;
				else if (Name == "LumpSize") OutField.LumpSize = V;
				else if (Name == "DetailStrength") OutField.DetailStrength = V;
				else if (Name == "DetailSize") OutField.DetailSize = V;
				else if (Name == "ChamberNoiseShare") OutField.ChamberNoiseShare = V;
				else if (Name == "PassageNoiseShare") OutField.PassageNoiseShare = V;
			}
			return true;
		}
		OutError = "no row named " + std::string(Row);
		return false;
	}
}
