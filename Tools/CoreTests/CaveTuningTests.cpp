#include "TestFramework.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/CaveTuningCsv.h"
#include "PaleotrenchCore/Seed.h"

namespace
{
	// Data/CaveTuning.csv, found from this source file's own path (the build passes absolute paths).
	std::string ReadTuningCsv()
	{
		std::string Path = __FILE__;
		const size_t Cut = Path.find_last_of("\\/");
		Path = Path.substr(0, Cut);                              // .../Tools/CoreTests
		Path = Path.substr(0, Path.find_last_of("\\/"));         // .../Tools
		Path = Path.substr(0, Path.find_last_of("\\/")) + "/Data/CaveTuning.csv";
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}
}

PT_TEST(CaveTuning_CsvParsesAndRejectsBadValues)
{
	pt::CaveLayoutTuning Layout;
	pt::CaveFieldTuning Field;
	std::string Error;
	const char* Csv = "Name,MinChambers,RegionHeight,LumpStrength,Unknown\nA,9,12345.5,77,1\nB,x,1,1,1\n";
	PT_EXPECT(pt::ReadCaveTuningRow(Csv, "A", Layout, Field, Error));
	PT_EXPECT(Layout.MinChambers == 9 && Layout.RegionHeight == 12345.5 && Field.LumpStrength == 77.0);
	PT_EXPECT(!pt::ReadCaveTuningRow(Csv, "B", Layout, Field, Error));
	PT_EXPECT(Error.find("MinChambers") != std::string::npos);
	PT_EXPECT(!pt::ReadCaveTuningRow(Csv, "C", Layout, Field, Error));
}

// Every boss cave's shipped tuning makes valid caves for hundreds of seeds, with open passages and domes.
PT_TEST(CaveTuning_EveryShippedCaveMeetsTheRules)
{
	const std::string Csv = ReadTuningCsv();
	PT_EXPECT(!Csv.empty());
	for (int CaveId = 1; CaveId <= 4; ++CaveId)
	{
		pt::CaveLayoutTuning Layout;
		pt::CaveFieldTuning FieldTuning;
		std::string Error;
		const bool bRead = pt::ReadCaveTuningRow(Csv, pt::CaveTuningRowName(CaveId), Layout, FieldTuning, Error);
		PT_EXPECT(bRead);
		if (!bRead)
		{
			std::printf("    %s: %s\n", pt::CaveTuningRowName(CaveId), Error.c_str());
			continue;
		}
		int Failed = 0;
		int MaxAttempts = 0;
		double Chambers = 0.0;
		double Air = 0.0;
		double Swim = 0.0;
		double Shafts = 0.0;
		const int Seeds = 300;
		for (int s = 0; s < Seeds; ++s)
		{
			const uint64_t Seed = pt::DeriveCaveSeed(static_cast<uint64_t>(s) * 104729u + 7u, static_cast<pt::ECave>(CaveId));
			const pt::CaveGraph Graph = pt::GenerateCaveGraph(Seed, Layout);
			const pt::CaveReport Report = pt::CheckCaveGraph(Graph, Layout);
			if (!Report.Ok())
			{
				if (Failed < 3)
				{
					std::printf("    %s seed %d: %s\n", pt::CaveTuningRowName(CaveId), s, Report.Problem.c_str());
				}
				++Failed;
				continue;
			}
			MaxAttempts = std::max(MaxAttempts, Graph.Attempts);
			Chambers += static_cast<double>(Graph.Nodes.size());
			Air += static_cast<double>(Graph.CountKind(pt::ECaveNodeKind::AirPocket));
			Swim += Graph.Nodes[static_cast<size_t>(Graph.Arena)].RouteDistance;
			Shafts += static_cast<double>(std::count_if(Graph.Passages.begin(), Graph.Passages.end(),
				[](const pt::CavePassage& P) { return P.Kind == pt::ECavePassageKind::Shaft; }));
			if (s < 3)
			{
				// The rock never closes a passage or an air dome.
				const pt::CaveField Field(Graph, FieldTuning);
				for (const pt::CavePassage& Passage : Graph.Passages)
				{
					for (const pt::Vec3& P : Passage.Path)
					{
						PT_EXPECT(Field.Sample(P) < -Passage.Radius * 0.3);
					}
				}
				for (const pt::CaveAirDome& Dome : Field.AirDomes())
				{
					PT_EXPECT(Field.Sample(Dome.WaterCentre + pt::Vec3(0.0, 0.0, 240.0)) < 0.0);
				}
			}
		}
		const double Ok = std::max(1.0, static_cast<double>(Seeds - Failed));
		std::printf("    %-13s %d seeds, %d failed, up to %d attempts: chambers %.1f, air pockets %.1f, shafts %.1f, swim to arena %.0f m\n",
			pt::CaveTuningRowName(CaveId), Seeds, Failed, MaxAttempts, Chambers / Ok, Air / Ok, Shafts / Ok, Swim / Ok / 100.0);
		PT_EXPECT(Failed == 0);
		PT_EXPECT(MaxAttempts <= 20);
	}
}
