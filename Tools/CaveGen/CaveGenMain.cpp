// cavegen: prints the layout of any cave seed (GDD 4, step 6), so a cave from a bug report can be
// rebuilt and inspected without Unreal.
//
//   cavegen <seed>                 a cave seed as a number (decimal or 0x hex) or as words
//   cavegen <seed> --cave <id>     treat <seed> as a SAVE seed and show cave <id> (1-4) of that save, as the game builds it
//   cavegen <seed> --row <id>      use cave <id>'s tuning row for a cave seed (default 1)
//   cavegen ... --tuning <csv>     read the tuning rows from this file (Tools/CaveGen.bat passes Data/CaveTuning.csv)
//   cavegen <seed> --svg map.svg   also write a top and side view as SVG
//   cavegen <seed> --attempts      list why rejected layouts were rejected
//   cavegen <seed> --survey <n>    statistics over n consecutive seeds starting at <seed>

// Plain fopen is fine for a command-line tool (MSVC would otherwise ask for fopen_s).
#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/CaveTuningCsv.h"
#include "PaleotrenchCore/Seed.h"

namespace
{
	char KindLetter(pt::ECaveNodeKind Kind)
	{
		switch (Kind)
		{
		case pt::ECaveNodeKind::Chamber:   return 'o';
		case pt::ECaveNodeKind::Entrance:  return 'E';
		case pt::ECaveNodeKind::AirPocket: return '@';
		case pt::ECaveNodeKind::Mineral:   return '$';
		case pt::ECaveNodeKind::Nest:      return 'n';
		case pt::ECaveNodeKind::Arena:     return 'A';
		}
		return '?';
	}

	const char* KindColour(pt::ECaveNodeKind Kind)
	{
		switch (Kind)
		{
		case pt::ECaveNodeKind::Chamber:   return "#8a9199";
		case pt::ECaveNodeKind::Entrance:  return "#4caf50";
		case pt::ECaveNodeKind::AirPocket: return "#29b6f6";
		case pt::ECaveNodeKind::Mineral:   return "#d4a017";
		case pt::ECaveNodeKind::Nest:      return "#9c27b0";
		case pt::ECaveNodeKind::Arena:     return "#e53935";
		}
		return "#000000";
	}

	char PassageLetter(pt::ECavePassageKind Kind)
	{
		switch (Kind)
		{
		case pt::ECavePassageKind::Tunnel:  return '.';
		case pt::ECavePassageKind::Squeeze: return ':';
		case pt::ECavePassageKind::Shaft:   return '+';
		}
		return '.';
	}

	uint64_t ParseSeed(const char* Text)
	{
		const size_t Len = std::strlen(Text);
		const bool bHex = Len > 2 && Text[0] == '0' && (Text[1] == 'x' || Text[1] == 'X');
		bool bDigits = Len > 0;
		for (size_t i = bHex ? 2 : 0; i < Len; ++i)
		{
			const char C = Text[i];
			const bool bOk = bHex ? std::isxdigit(static_cast<unsigned char>(C)) != 0 : (C >= '0' && C <= '9');
			bDigits = bDigits && bOk;
		}
		if (bDigits)
		{
			return std::strtoull(Text, nullptr, bHex ? 16 : 10);
		}
		return pt::HashText(Text);
	}

	// Draws the cave projected on two axes (0 = X, 1 = Y, 2 = Z) as text.
	void PrintView(const pt::CaveGraph& Graph, int AxisU, int AxisV, int Cols, int Rows, const char* Title)
	{
		const pt::Box3 Box = Graph.Bounds();
		auto Get = [](const pt::Vec3& P, int Axis) { return Axis == 0 ? P.X : (Axis == 1 ? P.Y : P.Z); };
		const double U0 = Get(Box.Min, AxisU);
		const double U1 = Get(Box.Max, AxisU);
		const double V0 = Get(Box.Min, AxisV);
		const double V1 = Get(Box.Max, AxisV);
		const double CellU = (U1 - U0) / Cols;
		const double CellV = (V1 - V0) / Rows;
		std::vector<std::string> Grid(static_cast<size_t>(Rows), std::string(static_cast<size_t>(Cols), ' '));
		auto Plot = [&](double U, double V, char C)
		{
			const int Col = std::clamp(static_cast<int>((U - U0) / CellU), 0, Cols - 1);
			const int Row = std::clamp(static_cast<int>((V1 - V) / CellV), 0, Rows - 1);  // up is up
			Grid[static_cast<size_t>(Row)][static_cast<size_t>(Col)] = C;
		};
		for (const pt::CavePassage& Passage : Graph.Passages)
		{
			for (size_t s = 0; s + 1 < Passage.Path.size(); ++s)
			{
				for (int k = 0; k < 4; ++k)
				{
					const pt::Vec3 P = pt::Vec3::Lerp(Passage.Path[s], Passage.Path[s + 1], k / 4.0);
					Plot(Get(P, AxisU), Get(P, AxisV), PassageLetter(Passage.Kind));
				}
			}
		}
		for (const pt::CaveNode& Node : Graph.Nodes)
		{
			const double Ru = Get(Node.Radii, AxisU);
			const double Rv = Get(Node.Radii, AxisV);
			for (double U = -Ru; U <= Ru; U += CellU * 0.5)
			{
				for (double V = -Rv; V <= Rv; V += CellV * 0.5)
				{
					if ((U * U) / (Ru * Ru) + (V * V) / (Rv * Rv) <= 1.0)
					{
						Plot(Get(Node.Centre, AxisU) + U, Get(Node.Centre, AxisV) + V, '#');
					}
				}
			}
		}
		for (const pt::CaveNode& Node : Graph.Nodes)
		{
			Plot(Get(Node.Centre, AxisU), Get(Node.Centre, AxisV), KindLetter(Node.Kind));
		}
		std::printf("\n%s (%.0f m x %.0f m)\n", Title, (U1 - U0) / 100.0, (V1 - V0) / 100.0);
		std::printf("+%s+\n", std::string(static_cast<size_t>(Cols), '-').c_str());
		for (const std::string& Line : Grid)
		{
			std::printf("|%s|\n", Line.c_str());
		}
		std::printf("+%s+\n", std::string(static_cast<size_t>(Cols), '-').c_str());
	}

	bool WriteSvg(const pt::CaveGraph& Graph, const char* Path)
	{
		FILE* File = std::fopen(Path, "w");
		if (!File)
		{
			return false;
		}
		const pt::Box3 Box = Graph.Bounds();
		const double Width = 1400.0;
		const double Scale = Width / (Box.Max.X - Box.Min.X);
		const double TopH = (Box.Max.Y - Box.Min.Y) * Scale;
		const double SideH = (Box.Max.Z - Box.Min.Z) * Scale;
		const double Gap = 60.0;
		std::fprintf(File, "<svg xmlns='http://www.w3.org/2000/svg' width='%.0f' height='%.0f' font-family='sans-serif' font-size='12'>\n",
			Width + 40.0, TopH + SideH + Gap * 2.0 + 60.0);
		std::fprintf(File, "<rect width='100%%' height='100%%' fill='#14181c'/>\n");
		std::fprintf(File, "<text x='20' y='24' fill='#dde'>Cave seed 0x%016llx: top view (X right, Y up), then side view (X right, Z up)</text>\n",
			static_cast<unsigned long long>(Graph.Seed));
		for (int View = 0; View < 2; ++View)
		{
			const double OffY = View == 0 ? 40.0 : 40.0 + TopH + Gap;
			const double ViewH = View == 0 ? TopH : SideH;
			auto Map = [&](const pt::Vec3& P, double& Sx, double& Sy)
			{
				Sx = 20.0 + (P.X - Box.Min.X) * Scale;
				const double V = View == 0 ? (P.Y - Box.Min.Y) : (P.Z - Box.Min.Z);
				Sy = OffY + ViewH - V * Scale;
			};
			for (const pt::CavePassage& Passage : Graph.Passages)
			{
				const char* Colour = Passage.Kind == pt::ECavePassageKind::Squeeze ? "#c98a4b"
					: (Passage.Kind == pt::ECavePassageKind::Shaft ? "#7fb069" : "#5d7f93");
				std::fprintf(File, "<polyline fill='none' stroke='%s' stroke-width='%.1f' stroke-linecap='round' stroke-opacity='0.9' points='",
					Colour, std::max(1.0, Passage.Radius * 2.0 * Scale));
				for (const pt::Vec3& P : Passage.Path)
				{
					double Sx = 0.0;
					double Sy = 0.0;
					Map(P, Sx, Sy);
					std::fprintf(File, "%.1f,%.1f ", Sx, Sy);
				}
				std::fprintf(File, "'/>\n");
			}
			for (size_t i = 0; i < Graph.Nodes.size(); ++i)
			{
				const pt::CaveNode& Node = Graph.Nodes[i];
				double Sx = 0.0;
				double Sy = 0.0;
				Map(Node.Centre, Sx, Sy);
				const double Rv = View == 0 ? Node.Radii.Y : Node.Radii.Z;
				std::fprintf(File, "<ellipse cx='%.1f' cy='%.1f' rx='%.1f' ry='%.1f' fill='%s' fill-opacity='0.75'/>\n", Sx, Sy,
					Node.Radii.X * Scale, Rv * Scale, KindColour(Node.Kind));
				std::fprintf(File, "<text x='%.1f' y='%.1f' fill='#fff' text-anchor='middle'>%zu</text>\n", Sx, Sy + 4.0, i);
			}
		}
		const struct { pt::ECaveNodeKind Kind; const char* Name; } Legend[] = {
			{pt::ECaveNodeKind::Entrance, "entrance"}, {pt::ECaveNodeKind::AirPocket, "air pocket"}, {pt::ECaveNodeKind::Chamber, "chamber"},
			{pt::ECaveNodeKind::Mineral, "mineral room"}, {pt::ECaveNodeKind::Nest, "nest"}, {pt::ECaveNodeKind::Arena, "arena"}};
		double Lx = 20.0;
		const double Ly = TopH + SideH + Gap * 2.0 + 40.0;
		for (const auto& Item : Legend)
		{
			std::fprintf(File, "<circle cx='%.0f' cy='%.0f' r='6' fill='%s'/><text x='%.0f' y='%.0f' fill='#dde'>%s</text>\n", Lx, Ly,
				KindColour(Item.Kind), Lx + 10.0, Ly + 4.0, Item.Name);
			Lx += 130.0;
		}
		std::fprintf(File, "<text x='%.0f' y='%.0f' fill='#5d7f93'>tunnel</text><text x='%.0f' y='%.0f' fill='#c98a4b'>squeeze</text>"
			"<text x='%.0f' y='%.0f' fill='#7fb069'>shaft</text>\n", Lx, Ly + 4.0, Lx + 60.0, Ly + 4.0, Lx + 130.0, Ly + 4.0);
		std::fprintf(File, "</svg>\n");
		std::fclose(File);
		return true;
	}

	void PrintSummary(const pt::CaveGraph& Graph, const pt::CaveLayoutTuning& Tuning)
	{
		const pt::CaveReport Report = pt::CheckCaveGraph(Graph, Tuning);
		int Kinds[3] = {0, 0, 0};
		int Loops = 0;
		double Total = 0.0;
		for (const pt::CavePassage& Passage : Graph.Passages)
		{
			++Kinds[static_cast<int>(Passage.Kind)];
			Loops += Passage.bLoop ? 1 : 0;
			Total += Passage.Length;
		}
		const pt::Box3 Box = Graph.Bounds();
		std::printf("Cave seed 0x%016llx (layout attempt %d)\n", static_cast<unsigned long long>(Graph.Seed), Graph.Attempts);
		std::printf("  size %.0f x %.0f x %.0f m\n", (Box.Max.X - Box.Min.X) / 100.0, (Box.Max.Y - Box.Min.Y) / 100.0,
			(Box.Max.Z - Box.Min.Z) / 100.0);
		std::printf("  %zu chambers: 1 entrance, %d air pockets, %d chambers, %d mineral rooms, %d nests, 1 arena\n", Graph.Nodes.size(),
			Graph.CountKind(pt::ECaveNodeKind::AirPocket), Graph.CountKind(pt::ECaveNodeKind::Chamber),
			Graph.CountKind(pt::ECaveNodeKind::Mineral), Graph.CountKind(pt::ECaveNodeKind::Nest));
		std::printf("  %zu passages (%d tunnels, %d squeezes, %d shafts; %d are alternative routes), %.0f m in total\n",
			Graph.Passages.size(), Kinds[0], Kinds[1], Kinds[2], Loops, Total / 100.0);
		if (Graph.Arena >= 0)
		{
			std::printf("  shortest swim entrance -> arena %.0f m; longest swim to air %.0f m (limit %.0f m)\n",
				Graph.Nodes[static_cast<size_t>(Graph.Arena)].RouteDistance / 100.0, Report.LongestSwimToAir / 100.0, Tuning.MaxAirSwim / 100.0);
		}
		std::printf("  rules: %s%s\n", Report.Ok() ? "all met" : "BROKEN: ", Report.Ok() ? "" : Report.Problem.c_str());
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::printf("usage: cavegen <seed or words> [--cave 1-4] [--svg file.svg] [--attempts] [--survey count]\n");
		return 2;
	}
	uint64_t Seed = ParseSeed(Argv[1]);
	const char* SvgPath = nullptr;
	const char* TuningPath = nullptr;
	bool bAttempts = false;
	int Survey = 0;
	int Row = 1;
	for (int i = 2; i < Argc; ++i)
	{
		if (std::strcmp(Argv[i], "--cave") == 0 && i + 1 < Argc)
		{
			Row = std::atoi(Argv[++i]);
			Seed = pt::DeriveCaveSeed(Seed, static_cast<pt::ECave>(Row));
		}
		else if (std::strcmp(Argv[i], "--row") == 0 && i + 1 < Argc)
		{
			Row = std::atoi(Argv[++i]);
		}
		else if (std::strcmp(Argv[i], "--tuning") == 0 && i + 1 < Argc)
		{
			TuningPath = Argv[++i];
		}
		else if (std::strcmp(Argv[i], "--svg") == 0 && i + 1 < Argc)
		{
			SvgPath = Argv[++i];
		}
		else if (std::strcmp(Argv[i], "--attempts") == 0)
		{
			bAttempts = true;
		}
		else if (std::strcmp(Argv[i], "--survey") == 0 && i + 1 < Argc)
		{
			Survey = std::atoi(Argv[++i]);
		}
	}
	pt::CaveLayoutTuning Tuning;
	pt::CaveFieldTuning FieldTuning;
	if (TuningPath)
	{
		std::ifstream File(TuningPath, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		std::string Error;
		if (!pt::ReadCaveTuningRow(Text.str(), pt::CaveTuningRowName(Row), Tuning, FieldTuning, Error))
		{
			std::printf("could not read %s: %s
", TuningPath, Error.c_str());
			return 2;
		}
		std::printf("tuning: %s row of %s
", pt::CaveTuningRowName(Row), TuningPath);
	}
	if (Survey > 0)
	{
		int Broken = 0;
		int WorstAttempts = 0;
		std::vector<int> AttemptCounts(41, 0);
		for (int n = 0; n < Survey; ++n)
		{
			const pt::CaveGraph Graph = pt::GenerateCaveGraph(Seed + static_cast<uint64_t>(n), Tuning);
			const bool bOk = pt::CheckCaveGraph(Graph, Tuning).Ok();
			Broken += bOk ? 0 : 1;
			WorstAttempts = std::max(WorstAttempts, Graph.Attempts);
			++AttemptCounts[static_cast<size_t>(std::min(Graph.Attempts, 40))];
		}
		std::printf("%d seeds from 0x%016llx: %d broken, worst %d attempts\n", Survey, static_cast<unsigned long long>(Seed), Broken, WorstAttempts);
		for (size_t a = 1; a < AttemptCounts.size(); ++a)
		{
			if (AttemptCounts[a] > 0)
			{
				std::printf("  %2zu attempt(s): %d seeds\n", a, AttemptCounts[a]);
			}
		}
		return Broken == 0 ? 0 : 1;
	}
	std::vector<std::string> Log;
	const pt::CaveGraph Graph = pt::GenerateCaveGraph(Seed, Tuning, bAttempts ? &Log : nullptr);
	PrintSummary(Graph, Tuning);
	for (size_t a = 0; a < Log.size(); ++a)
	{
		std::printf("  attempt %zu rejected: %s\n", a + 1, Log[a].c_str());
	}
	PrintView(Graph, 0, 1, 110, 40, "Top view: X across, Y up");
	PrintView(Graph, 0, 2, 110, 14, "Side view: X across, Z up");
	std::printf("legend: E entrance  @ air pocket  o chamber  $ mineral room  n nest  A arena  . tunnel  : squeeze  + shaft\n");
	if (SvgPath)
	{
		std::printf(WriteSvg(Graph, SvgPath) ? "wrote %s\n" : "could not write %s\n", SvgPath);
	}
	return pt::CheckCaveGraph(Graph, Tuning).Ok() ? 0 : 1;
}
