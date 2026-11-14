#include "TestFramework.h"

#include <algorithm>
#include <cmath>

#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/Seed.h"

namespace
{
	bool SameCave(const pt::CaveGraph& A, const pt::CaveGraph& B)
	{
		if (A.Nodes.size() != B.Nodes.size() || A.Passages.size() != B.Passages.size() || A.Entrance != B.Entrance
			|| A.Arena != B.Arena)
		{
			return false;
		}
		for (size_t i = 0; i < A.Nodes.size(); ++i)
		{
			const pt::CaveNode& Na = A.Nodes[i];
			const pt::CaveNode& Nb = B.Nodes[i];
			if (Na.Kind != Nb.Kind || Na.Centre.X != Nb.Centre.X || Na.Centre.Y != Nb.Centre.Y || Na.Centre.Z != Nb.Centre.Z
				|| Na.Radii.X != Nb.Radii.X || Na.Radii.Z != Nb.Radii.Z)
			{
				return false;
			}
		}
		for (size_t p = 0; p < A.Passages.size(); ++p)
		{
			const pt::CavePassage& Pa = A.Passages[p];
			const pt::CavePassage& Pb = B.Passages[p];
			if (Pa.A != Pb.A || Pa.B != Pb.B || Pa.Kind != Pb.Kind || Pa.Radius != Pb.Radius || Pa.Path.size() != Pb.Path.size()
				|| Pa.Length != Pb.Length)
			{
				return false;
			}
		}
		return true;
	}
}

PT_TEST(CaveGraph_SameSeedGivesTheSameCave)
{
	const pt::CaveLayoutTuning Tuning;
	for (uint64_t Seed : {1ull, 42ull, pt::HashText("drowned cathedral")})
	{
		PT_EXPECT(SameCave(pt::GenerateCaveGraph(Seed, Tuning), pt::GenerateCaveGraph(Seed, Tuning)));
	}
}

PT_TEST(CaveGraph_DifferentSeedsGiveDifferentCaves)
{
	const pt::CaveLayoutTuning Tuning;
	int Same = 0;
	for (uint64_t Seed = 0; Seed < 50; ++Seed)
	{
		Same += SameCave(pt::GenerateCaveGraph(Seed, Tuning), pt::GenerateCaveGraph(Seed + 1000, Tuning)) ? 1 : 0;
	}
	PT_EXPECT(Same == 0);
}

// The GDD's rules for every cave, over thousands of seeds: connected, two routes to the arena, air in
// reach everywhere, the arena the farthest swim, chambers apart and passages clear of other chambers.
PT_TEST(CaveGraph_ThousandsOfSeedsMeetEveryRule)
{
	const pt::CaveLayoutTuning Tuning;
	int Failed = 0;
	int MaxAttempts = 0;
	double Chambers = 0.0;
	double Air = 0.0;
	double Loops = 0.0;
	double ArenaSwim = 0.0;
	int MinChambers = 1000;
	const int Seeds = 2000;
	for (int s = 0; s < Seeds; ++s)
	{
		const uint64_t Seed = pt::DeriveCaveSeed(static_cast<uint64_t>(s) * 7919u, pt::ECave::Livyatan);
		const pt::CaveGraph Graph = pt::GenerateCaveGraph(Seed, Tuning);
		const pt::CaveReport Report = pt::CheckCaveGraph(Graph, Tuning);
		if (!Report.Ok())
		{
			if (Failed < 5)
			{
				std::printf("    seed %d failed: %s\n", s, Report.Problem.c_str());
			}
			++Failed;
			continue;
		}
		MaxAttempts = std::max(MaxAttempts, Graph.Attempts);
		MinChambers = std::min(MinChambers, static_cast<int>(Graph.Nodes.size()));
		Chambers += static_cast<double>(Graph.Nodes.size());
		Air += static_cast<double>(Graph.CountKind(pt::ECaveNodeKind::AirPocket));
		Loops += static_cast<double>(std::count_if(Graph.Passages.begin(), Graph.Passages.end(), [](const pt::CavePassage& P) { return P.bLoop; }));
		ArenaSwim += Graph.Nodes[static_cast<size_t>(Graph.Arena)].RouteDistance;
	}
	const double Ok = static_cast<double>(Seeds - Failed);
	std::printf("    %d seeds: %d failed; up to %d attempts; chambers avg %.1f (min %d); air pockets avg %.1f; "
		"alternative passages avg %.1f; swim to arena avg %.0f m\n", Seeds, Failed, MaxAttempts, Chambers / Ok, MinChambers,
		Air / Ok, Loops / Ok, ArenaSwim / Ok / 100.0);
	PT_EXPECT(Failed == 0);
	PT_EXPECT(MaxAttempts <= 20);
}

PT_TEST(CaveGraph_LayoutHasTheExpectedShape)
{
	const pt::CaveLayoutTuning Tuning;
	const pt::CaveGraph Graph = pt::GenerateCaveGraph(pt::HashText("shape"), Tuning);
	PT_EXPECT(Graph.CountKind(pt::ECaveNodeKind::Entrance) == 1);
	PT_EXPECT(Graph.CountKind(pt::ECaveNodeKind::Arena) == 1);
	const pt::CaveNode& Entrance = Graph.Nodes[static_cast<size_t>(Graph.Entrance)];
	const pt::CaveNode& Arena = Graph.Nodes[static_cast<size_t>(Graph.Arena)];
	PT_EXPECT(Entrance.HasAir());
	PT_EXPECT(!Arena.HasAir());
	PT_EXPECT(Entrance.Centre.X < Arena.Centre.X);
	PT_EXPECT(Arena.BoundRadius() >= Tuning.ArenaRadius);
	PT_EXPECT(Entrance.RouteDistance == 0.0);
	for (const pt::CavePassage& Passage : Graph.Passages)
	{
		// Paths run centre to centre and are at least as long as the straight line.
		const pt::Vec3& Ca = Graph.Nodes[static_cast<size_t>(Passage.A)].Centre;
		const pt::Vec3& Cb = Graph.Nodes[static_cast<size_t>(Passage.B)].Centre;
		PT_EXPECT((Passage.Path.front() - Ca).Length() < 1e-6);
		PT_EXPECT((Passage.Path.back() - Cb).Length() < 1e-6);
		PT_EXPECT(Passage.Length >= (Cb - Ca).Length() - 1e-6);
		PT_EXPECT(Passage.Radius >= Tuning.SqueezeRadius * 0.85);
		if (Passage.Kind == pt::ECavePassageKind::Squeeze)
		{
			PT_EXPECT(Passage.bLoop);
		}
	}
	const pt::Box3 Box = Graph.Bounds();
	PT_EXPECT(Box.Max.X - Box.Min.X <= Tuning.RegionLength + 2.0 * Tuning.TunnelRadiusMax * 1.1 + 1.0);
}

PT_TEST(CaveGraph_CheckerCatchesBrokenCaves)
{
	const pt::CaveLayoutTuning Tuning;
	const pt::CaveGraph Good = pt::GenerateCaveGraph(pt::HashText("checker"), Tuning);
	PT_EXPECT(pt::CheckCaveGraph(Good, Tuning).Ok());

	// Without any alternative routes some passage must cut the arena off.
	{
		pt::CaveGraph Tree = Good;
		Tree.Passages.erase(std::remove_if(Tree.Passages.begin(), Tree.Passages.end(), [](const pt::CavePassage& P) { return P.bLoop; }),
			Tree.Passages.end());
		const pt::CaveReport Report = pt::CheckCaveGraph(Tree, Tuning);
		PT_EXPECT(!Report.bTwoRoutesToArena || !Report.bConnected);
	}
	// Air pockets removed: the far end is out of reach.
	{
		pt::CaveGraph NoAir = Good;
		for (pt::CaveNode& Node : NoAir.Nodes)
		{
			if (Node.Kind == pt::ECaveNodeKind::AirPocket)
			{
				Node.Kind = pt::ECaveNodeKind::Chamber;
			}
		}
		if (Good.CountKind(pt::ECaveNodeKind::AirPocket) > 0)
		{
			PT_EXPECT(!pt::CheckCaveGraph(NoAir, Tuning).bAirInReach);
		}
	}
	// A chamber moved onto another one.
	{
		pt::CaveGraph Overlap = Good;
		Overlap.Nodes[2].Centre = Overlap.Nodes[3].Centre;
		PT_EXPECT(!pt::CheckCaveGraph(Overlap, Tuning).bChambersApart);
	}
	// A dead end pushed far past the arena.
	{
		pt::CaveGraph Far = Good;
		pt::CavePassage& Passage = Far.Passages.front();
		Passage.Length += 1.0e7;
		const pt::CaveReport Report = pt::CheckCaveGraph(Far, Tuning);
		PT_EXPECT(!Report.Ok());
	}
	// A disconnected chamber.
	{
		pt::CaveGraph Island = Good;
		pt::CaveNode Lonely = Island.Nodes[2];
		Lonely.Centre = pt::Vec3(-1.0e6, -1.0e6, 0.0);
		Island.Nodes.push_back(Lonely);
		PT_EXPECT(!pt::CheckCaveGraph(Island, Tuning).bConnected);
	}
}

PT_TEST(CaveGraph_RoutesAndDistancesAgree)
{
	const pt::CaveLayoutTuning Tuning;
	const pt::CaveGraph Graph = pt::GenerateCaveGraph(pt::HashText("routes"), Tuning);
	const std::vector<double> FromEntrance = pt::CaveRouteDistances(Graph, Graph.Entrance);
	const std::vector<int> Route = pt::CaveRoute(Graph, Graph.Entrance, Graph.Arena);
	double Sum = 0.0;
	int Node = Graph.Entrance;
	for (const int p : Route)
	{
		const pt::CavePassage& Passage = Graph.Passages[static_cast<size_t>(p)];
		PT_EXPECT(Passage.A == Node || Passage.B == Node);
		Node = Passage.Other(Node);
		Sum += Passage.Length;
	}
	PT_EXPECT(Node == Graph.Arena);
	PT_EXPECT(std::fabs(Sum - FromEntrance[static_cast<size_t>(Graph.Arena)]) < 1e-6);
	const std::vector<double> Air = pt::CaveAirDistances(Graph);
	for (size_t i = 0; i < Graph.Nodes.size(); ++i)
	{
		PT_EXPECT(Air[i] >= 0.0);
		PT_EXPECT((Air[i] == 0.0) == Graph.Nodes[i].HasAir());
		PT_EXPECT(std::fabs(Graph.Nodes[i].AirDistance - Air[i]) < 1e-6);
		PT_EXPECT(std::fabs(Graph.Nodes[i].RouteDistance - FromEntrance[i]) < 1e-6);
	}
}
