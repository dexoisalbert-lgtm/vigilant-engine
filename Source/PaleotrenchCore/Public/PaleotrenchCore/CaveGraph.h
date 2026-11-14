#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "PaleotrenchCore/Api.h"
#include "PaleotrenchCore/Vec3.h"

namespace pt
{
	// Step 1 of cave generation (GDD 4): the layout as a graph of chambers
	// joined by passages. Everything is decided from the seed, so a save's
	// caves never change. Units are cm in the cave's own space: the entrance
	// end is near X = 0, the arena near X = RegionLength, Z is up.

	enum class ECaveNodeKind : uint8_t
	{
		Chamber = 0,
		Entrance = 1,   // where the dive starts; has air (the first checkpoint)
		AirPocket = 2,  // air trapped under the ceiling: refill and checkpoint
		Mineral = 3,    // dead-end room reserved for mining (M5)
		Nest = 4,       // dead-end room reserved for creatures (M4)
		Arena = 5,      // the boss chamber, the farthest swim from the entrance
	};

	enum class ECavePassageKind : uint8_t
	{
		Tunnel = 0,
		Squeeze = 1,  // a tight alternative route
		Shaft = 2,    // steep enough to swim mostly up or down
	};

	PT_API const char* CaveNodeKindName(ECaveNodeKind Kind);
	PT_API const char* CavePassageKindName(ECavePassageKind Kind);

	struct CaveNode
	{
		Vec3 Centre;
		Vec3 Radii;                  // half-sizes of the chamber: an ellipsoid cut by a flat silt floor
		ECaveNodeKind Kind = ECaveNodeKind::Chamber;
		double RouteDistance = 0.0;  // shortest swim along passages from the entrance (centre to centre)
		double AirDistance = 0.0;    // shortest swim to a chamber with air

		bool HasAir() const { return Kind == ECaveNodeKind::Entrance || Kind == ECaveNodeKind::AirPocket; }
		PT_API double BoundRadius() const;  // radius of a sphere that contains the chamber
		PT_API double FloorZ() const;       // the flat floor, part-way down the ellipsoid
		double CeilingZ() const { return Centre.Z + Radii.Z; }
	};

	struct CavePassage
	{
		int A = -1;
		int B = -1;
		ECavePassageKind Kind = ECavePassageKind::Tunnel;
		double Radius = 0.0;          // cm
		std::vector<Vec3> Path;       // centreline from chamber A's centre to chamber B's centre
		double Length = 0.0;          // along Path
		bool bLoop = false;           // added on top of the spanning tree as an alternative route

		int Other(int Node) const { return Node == A ? B : A; }
	};

	// Everything tunable about a cave's layout (loaded from Data/CaveTuning.csv in the game).
	struct CaveLayoutTuning
	{
		int MinChambers = 16;
		int MaxChambers = 24;
		double RegionLength = 48000.0;    // along X, entrance end to arena end, cm
		double RegionWidth = 30000.0;     // along Y
		double RegionHeight = 9000.0;     // along Z
		double ChamberRadiusMin = 600.0;
		double ChamberRadiusMax = 1500.0;
		double ArenaRadius = 2600.0;
		double ChamberGap = 1200.0;       // minimum rock between two chambers
		double TunnelRadiusMin = 190.0;
		double TunnelRadiusMax = 300.0;
		double SqueezeRadius = 120.0;
		double SqueezeChance = 0.25;      // share of alternative-route passages that are squeezes
		double ShaftSlope = 1.0;          // rise over run above which a passage counts as a shaft
		double MaxPassageLength = 9000.0; // centre to centre
		int CandidateNeighbours = 5;      // nearest chambers considered for alternative routes
		double LoopFraction = 0.3;        // extra passages per chamber on top of the spanning tree
		double MaxAirSwim = 18000.0;      // longest swim from any point to the nearest air
		double SideRoomChance = 0.6;      // dead ends that become mineral rooms or nests
	};

	struct CaveGraph
	{
		uint64_t Seed = 0;
		std::vector<CaveNode> Nodes;
		std::vector<CavePassage> Passages;
		int Entrance = -1;
		int Arena = -1;
		int Attempts = 0;  // layouts tried before one met every rule (deterministic per seed)

		// Box around every chamber and passage including their radii.
		PT_API Box3 Bounds() const;
		PT_API int CountKind(ECaveNodeKind Kind) const;
	};

	// Builds the layout for a seed. Always returns a cave; if no attempt meets
	// every rule (never seen in tests) the last attempt is returned and
	// CheckCaveGraph says why.
	// AttemptLog (optional) receives, for each rejected attempt, the rule it broke.
	PT_API CaveGraph GenerateCaveGraph(uint64_t Seed, const CaveLayoutTuning& Tuning, std::vector<std::string>* AttemptLog = nullptr);

	struct CaveReport
	{
		bool bEnoughChambers = false;
		bool bConnected = false;
		bool bTwoRoutesToArena = false;   // no single passage can cut the entrance off from the arena
		bool bAirInReach = false;
		bool bArenaFarthest = false;
		bool bChambersApart = false;
		bool bPassagesClear = false;      // passages do not run through other chambers
		double LongestSwimToAir = 0.0;    // cm, including points part-way along passages
		std::string Problem;              // first rule that failed

		bool Ok() const
		{
			return bEnoughChambers && bConnected && bTwoRoutesToArena && bAirInReach && bArenaFarthest
				&& bChambersApart && bPassagesClear;
		}
	};

	PT_API CaveReport CheckCaveGraph(const CaveGraph& Graph, const CaveLayoutTuning& Tuning);

	// Shortest swim (sum of passage lengths) from Source to every chamber; -1
	// where unreachable. SkipPassage lets a caller test the cave without one passage.
	PT_API std::vector<double> CaveRouteDistances(const CaveGraph& Graph, int Source, int SkipPassage = -1);

	// Shortest swim from every chamber to the nearest chamber with air.
	PT_API std::vector<double> CaveAirDistances(const CaveGraph& Graph);

	// Passages of the shortest route From -> To, in order (empty if unreachable).
	PT_API std::vector<int> CaveRoute(const CaveGraph& Graph, int From, int To);
}
