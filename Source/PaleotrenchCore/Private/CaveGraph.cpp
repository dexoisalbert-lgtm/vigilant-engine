#include "PaleotrenchCore/CaveGraph.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <utility>

#include "PaleotrenchCore/Random.h"
#include "PaleotrenchCore/Seed.h"

namespace pt
{
	namespace
	{
		constexpr int MaxLayoutAttempts = 40;
		constexpr double Unreachable = std::numeric_limits<double>::infinity();
		// Rock kept between a passage and any chamber it does not connect, on top of both radii.
		constexpr double PassageClearance = 500.0;
		// The checker allows passages a little closer than generation aims for.
		constexpr double CheckedClearance = PassageClearance * 0.5;

		double RangeD(Rng& R, double Lo, double Hi)
		{
			return Lo + (Hi - Lo) * static_cast<double>(R.Float01());
		}

		struct Candidate
		{
			int A = -1;
			int B = -1;
			double Distance = 0.0;  // centre to centre
		};

		// Distances from a set of sources along passages; Via[i] is the passage used to reach node i.
		std::vector<double> Dijkstra(const CaveGraph& Graph, const std::vector<int>& Sources, int SkipPassage, std::vector<int>* Via)
		{
			const size_t Count = Graph.Nodes.size();
			std::vector<std::vector<int>> Links(Count);
			for (size_t p = 0; p < Graph.Passages.size(); ++p)
			{
				if (static_cast<int>(p) == SkipPassage)
				{
					continue;
				}
				const CavePassage& Passage = Graph.Passages[p];
				Links[static_cast<size_t>(Passage.A)].push_back(static_cast<int>(p));
				Links[static_cast<size_t>(Passage.B)].push_back(static_cast<int>(p));
			}
			std::vector<double> Dist(Count, Unreachable);
			if (Via)
			{
				Via->assign(Count, -1);
			}
			using Entry = std::pair<double, int>;
			std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Open;
			for (const int Source : Sources)
			{
				if (Source >= 0 && static_cast<size_t>(Source) < Count)
				{
					Dist[static_cast<size_t>(Source)] = 0.0;
					Open.push({0.0, Source});
				}
			}
			while (!Open.empty())
			{
				const auto [Here, Node] = Open.top();
				Open.pop();
				if (Here > Dist[static_cast<size_t>(Node)])
				{
					continue;
				}
				for (const int p : Links[static_cast<size_t>(Node)])
				{
					const CavePassage& Passage = Graph.Passages[static_cast<size_t>(p)];
					const int Next = Passage.Other(Node);
					const double There = Here + Passage.Length;
					if (There < Dist[static_cast<size_t>(Next)])
					{
						Dist[static_cast<size_t>(Next)] = There;
						if (Via)
						{
							(*Via)[static_cast<size_t>(Next)] = p;
						}
						Open.push({There, Next});
					}
				}
			}
			return Dist;
		}

		std::vector<int> AirNodes(const CaveGraph& Graph)
		{
			std::vector<int> Air;
			for (size_t i = 0; i < Graph.Nodes.size(); ++i)
			{
				if (Graph.Nodes[i].HasAir())
				{
					Air.push_back(static_cast<int>(i));
				}
			}
			return Air;
		}

		// Longest swim to air anywhere: chambers, and the worst point part-way along each passage.
		double LongestSwimToAir(const CaveGraph& Graph, const std::vector<double>& AirDist)
		{
			double Worst = 0.0;
			for (const double D : AirDist)
			{
				Worst = std::fmax(Worst, D);
			}
			for (const CavePassage& Passage : Graph.Passages)
			{
				const double Da = AirDist[static_cast<size_t>(Passage.A)];
				const double Db = AirDist[static_cast<size_t>(Passage.B)];
				Worst = std::fmax(Worst, (Da + Db + Passage.Length) * 0.5);
			}
			return Worst;
		}

		bool StraightPassageClear(const CaveGraph& Graph, int A, int B, double Radius)
		{
			const Vec3& Ca = Graph.Nodes[static_cast<size_t>(A)].Centre;
			const Vec3& Cb = Graph.Nodes[static_cast<size_t>(B)].Centre;
			for (size_t k = 0; k < Graph.Nodes.size(); ++k)
			{
				if (static_cast<int>(k) == A || static_cast<int>(k) == B)
				{
					continue;
				}
				const CaveNode& Other = Graph.Nodes[k];
				if (SegmentDistance(Other.Centre, Ca, Cb) < Other.BoundRadius() + Radius + PassageClearance)
				{
					return false;
				}
			}
			return true;
		}

		bool PathClear(const CaveGraph& Graph, const CavePassage& Passage, double Clearance)
		{
			for (size_t k = 0; k < Graph.Nodes.size(); ++k)
			{
				if (static_cast<int>(k) == Passage.A || static_cast<int>(k) == Passage.B)
				{
					continue;
				}
				const CaveNode& Other = Graph.Nodes[k];
				const double Limit = Other.BoundRadius() + Passage.Radius + Clearance;
				for (size_t s = 0; s + 1 < Passage.Path.size(); ++s)
				{
					if (SegmentDistance(Other.Centre, Passage.Path[s], Passage.Path[s + 1]) < Limit)
					{
						return false;
					}
				}
			}
			return true;
		}

		Vec3 Bezier(const Vec3& P0, const Vec3& P1, const Vec3& P2, const Vec3& P3, double T)
		{
			const double U = 1.0 - T;
			return P0 * (U * U * U) + P1 * (3.0 * U * U * T) + P2 * (3.0 * U * T * T) + P3 * (T * T * T);
		}

		CavePassage MakePassage(Rng& R, const CaveGraph& Graph, int A, int B, bool bLoop, const CaveLayoutTuning& Tuning)
		{
			CavePassage Passage;
			Passage.A = A;
			Passage.B = B;
			Passage.bLoop = bLoop;
			const Vec3 Ca = Graph.Nodes[static_cast<size_t>(A)].Centre;
			const Vec3 Cb = Graph.Nodes[static_cast<size_t>(B)].Centre;
			const Vec3 D = Cb - Ca;
			const double Len = D.Length();
			const double Run = std::sqrt(D.X * D.X + D.Y * D.Y);
			const bool bSqueeze = bLoop && R.Chance(static_cast<float>(Tuning.SqueezeChance));
			const double TunnelRadius = RangeD(R, Tuning.TunnelRadiusMin, Tuning.TunnelRadiusMax);
			if (bSqueeze)
			{
				Passage.Kind = ECavePassageKind::Squeeze;
				Passage.Radius = Tuning.SqueezeRadius * RangeD(R, 0.9, 1.1);
			}
			else if (std::fabs(D.Z) > Run * Tuning.ShaftSlope)
			{
				Passage.Kind = ECavePassageKind::Shaft;
				Passage.Radius = TunnelRadius * 1.1;
			}
			else
			{
				Passage.Kind = ECavePassageKind::Tunnel;
				Passage.Radius = TunnelRadius;
			}

			// A gentle S-bend: two control points pushed sideways (and a little up or down).
			const Vec3 Dir = Len > 1e-9 ? D / Len : Vec3(1.0, 0.0, 0.0);
			Vec3 Side = Dir.Cross(Vec3(0.0, 0.0, 1.0));
			if (Side.LengthSq() < 1e-6)
			{
				Side = Vec3(1.0, 0.0, 0.0);
			}
			Side = Side.Normalized();
			const Vec3 Up = Side.Cross(Dir).Normalized();
			const double Bend1 = RangeD(R, -0.22, 0.22) * Len;
			const double Bend2 = RangeD(R, -0.22, 0.22) * Len;
			const double Lift1 = RangeD(R, -0.08, 0.08) * Len;
			const double Lift2 = RangeD(R, -0.08, 0.08) * Len;
			const int Steps = std::max(4, static_cast<int>(std::ceil(Len / 250.0)));
			for (const double Scale : {1.0, 0.5, 0.0})
			{
				const Vec3 P1 = Ca + D * (1.0 / 3.0) + Side * (Bend1 * Scale) + Up * (Lift1 * Scale);
				const Vec3 P2 = Ca + D * (2.0 / 3.0) + Side * (Bend2 * Scale) + Up * (Lift2 * Scale);
				Passage.Path.clear();
				for (int s = 0; s <= Steps; ++s)
				{
					Passage.Path.push_back(Bezier(Ca, P1, P2, Cb, static_cast<double>(s) / static_cast<double>(Steps)));
				}
				if (Scale == 0.0 || PathClear(Graph, Passage, PassageClearance * 0.6))
				{
					break;
				}
			}
			Passage.Length = 0.0;
			for (size_t s = 0; s + 1 < Passage.Path.size(); ++s)
			{
				Passage.Length += (Passage.Path[s + 1] - Passage.Path[s]).Length();
			}
			return Passage;
		}

		CaveNode MakeChamber(Rng& R, const Vec3& Centre, double Radius, ECaveNodeKind Kind, double Flatness)
		{
			CaveNode Node;
			Node.Centre = Centre;
			Node.Kind = Kind;
			// One random draw per statement: argument evaluation order is unspecified in C++, and the
			// same seed must give the same cave with every compiler.
			const double Rx = Radius * RangeD(R, 0.85, 1.2);
			const double Ry = Radius * RangeD(R, 0.85, 1.2);
			Node.Radii = Vec3(Rx, Ry, Radius * Flatness);
			return Node;
		}

		// Passages that could be dug: chamber pairs within reach whose straight line stays clear of every
		// other chamber. bNearestOnly keeps each chamber's few nearest (natural local connections);
		// otherwise every clear pair in reach is listed.
		std::vector<Candidate> FindCandidates(const CaveGraph& Graph, const CaveLayoutTuning& Tuning, bool bNearestOnly)
		{
			std::vector<Candidate> Found;
			const size_t Count = Graph.Nodes.size();
			for (size_t i = 0; i < Count; ++i)
			{
				std::vector<Candidate> Near;
				for (size_t j = 0; j < Count; ++j)
				{
					if (i == j)
					{
						continue;
					}
					const CaveNode& Ni = Graph.Nodes[i];
					const CaveNode& Nj = Graph.Nodes[j];
					const double Dist = (Nj.Centre - Ni.Centre).Length();
					if (Dist - Ni.BoundRadius() - Nj.BoundRadius() <= Tuning.MaxPassageLength)
					{
						Near.push_back({static_cast<int>(i), static_cast<int>(j), Dist});
					}
				}
				std::sort(Near.begin(), Near.end(), [](const Candidate& L, const Candidate& Rt)
				{
					return L.Distance < Rt.Distance || (L.Distance == Rt.Distance && L.B < Rt.B);
				});
				const size_t Keep = bNearestOnly ? std::min(Near.size(), static_cast<size_t>(std::max(1, Tuning.CandidateNeighbours)))
					: Near.size();
				for (size_t n = 0; n < Keep; ++n)
				{
					Candidate C = Near[n];
					if (C.A > C.B)
					{
						std::swap(C.A, C.B);
					}
					const bool bKnown = std::any_of(Found.begin(), Found.end(), [&C](const Candidate& F) { return F.A == C.A && F.B == C.B; });
					if (!bKnown && StraightPassageClear(Graph, C.A, C.B, Tuning.TunnelRadiusMax))
					{
						Found.push_back(C);
					}
				}
			}
			std::sort(Found.begin(), Found.end(), [](const Candidate& L, const Candidate& Rt)
			{
				return L.Distance < Rt.Distance || (L.Distance == Rt.Distance && (L.A < Rt.A || (L.A == Rt.A && L.B < Rt.B)));
			});
			return Found;
		}

		bool HasPassage(const CaveGraph& Graph, int A, int B)
		{
			return std::any_of(Graph.Passages.begin(), Graph.Passages.end(), [A, B](const CavePassage& P)
			{
				return (P.A == A && P.B == B) || (P.A == B && P.B == A);
			});
		}

		// Keeps only the chambers flagged in Keep, renumbering passages, entrance and arena.
		void KeepNodes(CaveGraph& Graph, const std::vector<bool>& Keep)
		{
			std::vector<int> NewIndex(Graph.Nodes.size(), -1);
			std::vector<CaveNode> Nodes;
			for (size_t i = 0; i < Graph.Nodes.size(); ++i)
			{
				if (Keep[i])
				{
					NewIndex[i] = static_cast<int>(Nodes.size());
					Nodes.push_back(Graph.Nodes[i]);
				}
			}
			std::vector<CavePassage> Passages;
			for (CavePassage& Passage : Graph.Passages)
			{
				const int A = NewIndex[static_cast<size_t>(Passage.A)];
				const int B = NewIndex[static_cast<size_t>(Passage.B)];
				if (A >= 0 && B >= 0)
				{
					Passage.A = A;
					Passage.B = B;
					Passages.push_back(std::move(Passage));
				}
			}
			Graph.Entrance = Graph.Entrance >= 0 ? NewIndex[static_cast<size_t>(Graph.Entrance)] : -1;
			Graph.Arena = Graph.Arena >= 0 ? NewIndex[static_cast<size_t>(Graph.Arena)] : -1;
			Graph.Nodes = std::move(Nodes);
			Graph.Passages = std::move(Passages);
		}

		int Find(std::vector<int>& Parent, int X)
		{
			while (Parent[static_cast<size_t>(X)] != X)
			{
				Parent[static_cast<size_t>(X)] = Parent[static_cast<size_t>(Parent[static_cast<size_t>(X)])];
				X = Parent[static_cast<size_t>(X)];
			}
			return X;
		}

		// Drops every chamber that is a longer swim from the entrance than the arena (dead ends past it).
		void CullBeyondArena(CaveGraph& Graph)
		{
			const std::vector<double> Dist = Dijkstra(Graph, {Graph.Entrance}, -1, nullptr);
			const double ArenaDist = Dist[static_cast<size_t>(Graph.Arena)];
			std::vector<bool> Keep(Graph.Nodes.size(), true);
			bool bAny = false;
			for (size_t i = 0; i < Graph.Nodes.size(); ++i)
			{
				if (static_cast<int>(i) != Graph.Arena && Dist[i] > ArenaDist)
				{
					Keep[i] = false;
					bAny = true;
				}
			}
			if (bAny)
			{
				KeepNodes(Graph, Keep);
			}
		}

		bool InsertBypass(Rng& R, CaveGraph& Graph, int Bridge, const CaveLayoutTuning& Tuning);

		// Adds alternative routes until no single passage separates the entrance from the arena.
		bool SecondRouteToArena(Rng& R, CaveGraph& Graph, const CaveLayoutTuning& Tuning)
		{
			for (int Round = 0; Round < 24; ++Round)
			{
				int Bridge = -1;
				for (const int p : CaveRoute(Graph, Graph.Entrance, Graph.Arena))
				{
					if (Dijkstra(Graph, {Graph.Entrance}, p, nullptr)[static_cast<size_t>(Graph.Arena)] == Unreachable)
					{
						Bridge = p;
						break;
					}
				}
				if (Bridge < 0)
				{
					return true;
				}
				const std::vector<double> Side = Dijkstra(Graph, {Graph.Entrance}, Bridge, nullptr);
				const CavePassage& Cut = Graph.Passages[static_cast<size_t>(Bridge)];
				const int CutA = Cut.A;
				const int CutB = Cut.B;
				bool bAdded = false;
				for (const Candidate& C : FindCandidates(Graph, Tuning, false))
				{
					const bool bAcross = (Side[static_cast<size_t>(C.A)] == Unreachable) != (Side[static_cast<size_t>(C.B)] == Unreachable);
					const bool bSameAsCut = (C.A == CutA && C.B == CutB) || (C.A == CutB && C.B == CutA);
					if (bAcross && !bSameAsCut && !HasPassage(Graph, C.A, C.B))
					{
						Graph.Passages.push_back(MakePassage(R, Graph, C.A, C.B, true, Tuning));
						bAdded = true;
						break;
					}
				}
				if (!bAdded && !InsertBypass(R, Graph, Bridge, Tuning))
				{
					return false;
				}
			}
			return false;
		}

		// Turns chambers into air pockets, one at a time, each time picking the one that shortens the
		// longest swim to air the most, until every point is within reach.
		bool PlaceAir(CaveGraph& Graph, const CaveLayoutTuning& Tuning)
		{
			for (;;)
			{
				const double Worst = LongestSwimToAir(Graph, Dijkstra(Graph, AirNodes(Graph), -1, nullptr));
				if (Worst <= Tuning.MaxAirSwim)
				{
					return true;
				}
				int Best = -1;
				double BestWorst = Worst;
				for (size_t i = 0; i < Graph.Nodes.size(); ++i)
				{
					CaveNode& Node = Graph.Nodes[i];
					if (Node.Kind != ECaveNodeKind::Chamber)
					{
						continue;
					}
					Node.Kind = ECaveNodeKind::AirPocket;
					const double Trial = LongestSwimToAir(Graph, Dijkstra(Graph, AirNodes(Graph), -1, nullptr));
					Node.Kind = ECaveNodeKind::Chamber;
					if (Trial < BestWorst)
					{
						BestWorst = Trial;
						Best = static_cast<int>(i);
					}
				}
				if (Best < 0)
				{
					return false;
				}
				Graph.Nodes[static_cast<size_t>(Best)].Kind = ECaveNodeKind::AirPocket;
			}
		}

		// Inside the cave region and at least the gap away from every chamber.
		bool FitsInCave(const CaveGraph& Graph, const CaveNode& Node, const CaveLayoutTuning& Tuning)
		{
			const double Bound = Node.BoundRadius();
			const double Zr = Node.Radii.Z;
			if (Node.Centre.X < Bound || Node.Centre.X > Tuning.RegionLength - Bound
				|| std::fabs(Node.Centre.Y) > Tuning.RegionWidth / 2.0 - Bound
				|| std::fabs(Node.Centre.Z) > Tuning.RegionHeight / 2.0 - std::fmin(Zr, Tuning.RegionHeight / 2.0 - 1.0))
			{
				return false;
			}
			for (const CaveNode& Other : Graph.Nodes)
			{
				if ((Other.Centre - Node.Centre).Length() < Other.BoundRadius() + Bound + Tuning.ChamberGap)
				{
					return false;
				}
			}
			return true;
		}

		// When no existing chamber can bypass a passage that cuts the cave in two, dig a new side chamber
		// beside it with two passages around it.
		bool InsertBypass(Rng& R, CaveGraph& Graph, int Bridge, const CaveLayoutTuning& Tuning)
		{
			const CavePassage Cut = Graph.Passages[static_cast<size_t>(Bridge)];
			const Vec3 Ca = Graph.Nodes[static_cast<size_t>(Cut.A)].Centre;
			const Vec3 Cb = Graph.Nodes[static_cast<size_t>(Cut.B)].Centre;
			const Vec3 Mid = (Ca + Cb) * 0.5;
			const Vec3 Dir = (Cb - Ca).Normalized();
			Vec3 Side = Dir.Cross(Vec3(0.0, 0.0, 1.0));
			if (Side.LengthSq() < 1e-6)
			{
				Side = Vec3(1.0, 0.0, 0.0);
			}
			Side = Side.Normalized();
			const Vec3 Up = Side.Cross(Dir).Normalized();
			for (int Try = 0; Try < 32; ++Try)
			{
				const double Radius = RangeD(R, Tuning.ChamberRadiusMin, (Tuning.ChamberRadiusMin + Tuning.ChamberRadiusMax) * 0.5);
				const double Flatness = RangeD(R, 0.45, 0.75);
				CaveNode Node = MakeChamber(R, Vec3(), Radius, ECaveNodeKind::Chamber, Flatness);
				const double Angle = static_cast<double>(Try) * 0.785398 + RangeD(R, -0.3, 0.3);
				const double Away = RangeD(R, 1.0, 2.2) * (Node.BoundRadius() + Tuning.ChamberGap + Tuning.TunnelRadiusMax * 2.0);
				const Vec3 Offset = (Side * std::cos(Angle) + Up * (std::sin(Angle) * 0.5)).Normalized();
				Node.Centre = Mid + Offset * (Away + (Cb - Ca).Length() * 0.25);
				if (!FitsInCave(Graph, Node, Tuning))
				{
					continue;
				}
				Graph.Nodes.push_back(Node);
				const int New = static_cast<int>(Graph.Nodes.size()) - 1;
				const auto InReach = [&Graph, &Tuning](int A, int B)
				{
					const CaveNode& Na = Graph.Nodes[static_cast<size_t>(A)];
					const CaveNode& Nb = Graph.Nodes[static_cast<size_t>(B)];
					return (Na.Centre - Nb.Centre).Length() - Na.BoundRadius() - Nb.BoundRadius() <= Tuning.MaxPassageLength;
				};
				if (InReach(New, Cut.A) && InReach(New, Cut.B) && StraightPassageClear(Graph, New, Cut.A, Tuning.TunnelRadiusMax)
					&& StraightPassageClear(Graph, New, Cut.B, Tuning.TunnelRadiusMax))
				{
					// The new chamber must not block passages that already exist.
					bool bBlocks = false;
					for (const CavePassage& Passage : Graph.Passages)
					{
						if (!PathClear(Graph, Passage, PassageClearance * 0.6))
						{
							bBlocks = true;
							break;
						}
					}
					if (!bBlocks)
					{
						Graph.Passages.push_back(MakePassage(R, Graph, Cut.A, New, true, Tuning));
						Graph.Passages.push_back(MakePassage(R, Graph, New, Cut.B, true, Tuning));
						return true;
					}
				}
				Graph.Nodes.pop_back();
			}
			return false;
		}

		CaveGraph TryLayout(Rng& R, uint64_t Seed, const CaveLayoutTuning& Tuning)
		{
			CaveGraph Graph;
			Graph.Seed = Seed;
			const double L = Tuning.RegionLength;
			const double W = Tuning.RegionWidth;
			const double H = Tuning.RegionHeight;
			const double MidRadius = (Tuning.ChamberRadiusMin + Tuning.ChamberRadiusMax) * 0.5;

			// The entrance: near X = 0 and high up (closest to the surface).
			{
				const double Flatness = RangeD(R, 0.55, 0.7);
				CaveNode Entrance = MakeChamber(R, Vec3(), MidRadius, ECaveNodeKind::Entrance, Flatness);
				const double Y = RangeD(R, -W / 6.0, W / 6.0);
				Entrance.Centre = Vec3(Entrance.BoundRadius() * 1.2, Y, H / 2.0 - Entrance.Radii.Z - 200.0);
				Graph.Nodes.push_back(Entrance);
				Graph.Entrance = 0;
			}
			// The arena: big, at the far end and low down.
			{
				CaveNode Arena;
				Arena.Kind = ECaveNodeKind::Arena;
				const double Rx = Tuning.ArenaRadius * RangeD(R, 1.05, 1.25);
				const double Ry = Tuning.ArenaRadius * RangeD(R, 0.9, 1.1);
				const double Rz = Tuning.ArenaRadius * RangeD(R, 0.55, 0.7);
				Arena.Radii = Vec3(Rx, Ry, Rz);
				const double Y = RangeD(R, -W / 5.0, W / 5.0);
				const double Lift = RangeD(R, 0.0, H * 0.2);
				Arena.Centre = Vec3(L - Arena.BoundRadius() * 1.05, Y, -H / 2.0 + Arena.Radii.Z + Lift);
				Graph.Nodes.push_back(Arena);
				Graph.Arena = 1;
			}
			// The main route: chambers strung from the entrance towards the arena, wandering sideways and up
			// and down, so the two ends are always linked. Then side chambers grow off existing ones within
			// passage reach until the cave has its chambers. Never closer than the gap.
			const int Wanted = R.RangeInt(Tuning.MinChambers, Tuning.MaxChambers);
			const Vec3 From = Graph.Nodes[0].Centre;
			const Vec3 To = Graph.Nodes[1].Centre;
			const int SpineCount = std::max(1, static_cast<int>(std::lround((To - From).Length() / 6500.0)) - 1);
			double WanderY = 0.0;
			double WanderZ = 0.0;
			for (int j = 1; j <= SpineCount; ++j)
			{
				const double T = static_cast<double>(j) / static_cast<double>(SpineCount + 1);
				for (int Try = 0; Try < 12; ++Try)
				{
					const double StepY = RangeD(R, -0.12, 0.12) * W;
					const double StepZ = RangeD(R, -0.12, 0.12) * H;
					const double Y = std::clamp(WanderY + StepY, -W * 0.35, W * 0.35);
					const double Z = std::clamp(WanderZ + StepZ, -H * 0.3, H * 0.3);
					const double U = static_cast<double>(R.Float01());
					const double Radius = Tuning.ChamberRadiusMin + (Tuning.ChamberRadiusMax - Tuning.ChamberRadiusMin) * U;
					const double Flatness = RangeD(R, 0.45, 0.75);
					CaveNode Node = MakeChamber(R, Vec3(), Radius, ECaveNodeKind::Chamber, Flatness);
					const Vec3 Base = Vec3::Lerp(From, To, T);
					Node.Centre = Vec3(Base.X, Base.Y + Y, Base.Z + Z);
					if (FitsInCave(Graph, Node, Tuning))
					{
						Graph.Nodes.push_back(Node);
						WanderY = Y;
						WanderZ = Z;
						break;
					}
				}
			}
			for (int Try = 0; Try < Wanted * 80 && static_cast<int>(Graph.Nodes.size()) < Wanted; ++Try)
			{
				// Grow from any chamber but the arena (nothing should lie beyond it).
				const int Parent = R.RangeInt(0, static_cast<int>(Graph.Nodes.size()) - 1);
				if (Parent == Graph.Arena)
				{
					continue;
				}
				const double U = static_cast<double>(R.Float01());
				const double Radius = Tuning.ChamberRadiusMin + (Tuning.ChamberRadiusMax - Tuning.ChamberRadiusMin) * U * U;
				const bool bTall = R.Chance(0.12f);
				const double Flatness = bTall ? RangeD(R, 1.0, 1.35) : RangeD(R, 0.45, 0.75);
				CaveNode Node = MakeChamber(R, Vec3(), Radius, ECaveNodeKind::Chamber, Flatness);
				const double Dx = RangeD(R, -1.0, 1.0);
				const double Dy = RangeD(R, -1.0, 1.0);
				const double Dz = RangeD(R, -0.35, 0.35);
				const Vec3 Dir = Vec3(Dx, Dy, Dz).Normalized();
				const CaveNode& From2 = Graph.Nodes[static_cast<size_t>(Parent)];
				const double Reach = From2.BoundRadius() + Node.BoundRadius() + Tuning.ChamberGap
					+ RangeD(R, 0.0, Tuning.MaxPassageLength * 0.6);
				Node.Centre = From2.Centre + Dir * Reach;
				if (Dir.LengthSq() > 0.0 && FitsInCave(Graph, Node, Tuning))
				{
					Graph.Nodes.push_back(Node);
				}
			}

			// Keep only chambers that candidate passages can reach from the entrance.
			{
				const std::vector<Candidate> Reach = FindCandidates(Graph, Tuning, false);
				std::vector<int> Parent(Graph.Nodes.size());
				for (size_t i = 0; i < Parent.size(); ++i)
				{
					Parent[i] = static_cast<int>(i);
				}
				for (const Candidate& C : Reach)
				{
					Parent[static_cast<size_t>(Find(Parent, C.A))] = Find(Parent, C.B);
				}
				const int Root = Find(Parent, Graph.Entrance);
				if (Find(Parent, Graph.Arena) != Root)
				{
					return Graph;  // the arena cannot be reached: this attempt fails the checks
				}
				std::vector<bool> Keep(Graph.Nodes.size());
				for (size_t i = 0; i < Keep.size(); ++i)
				{
					Keep[i] = Find(Parent, static_cast<int>(i)) == Root;
				}
				KeepNodes(Graph, Keep);
			}

			// Spanning tree of the shortest clear passages (Kruskal), so every chamber is joined.
			{
				const std::vector<Candidate> Backbone = FindCandidates(Graph, Tuning, false);
				std::vector<int> Parent(Graph.Nodes.size());
				for (size_t i = 0; i < Parent.size(); ++i)
				{
					Parent[i] = static_cast<int>(i);
				}
				for (const Candidate& C : Backbone)
				{
					const int Ra = Find(Parent, C.A);
					const int Rb = Find(Parent, C.B);
					if (Ra != Rb)
					{
						Parent[static_cast<size_t>(Ra)] = Rb;
						Graph.Passages.push_back(MakePassage(R, Graph, C.A, C.B, false, Tuning));
					}
				}
			}

			// Alternative routes between near neighbours: favour pairs that are a long swim apart.
			const std::vector<Candidate> Candidates = FindCandidates(Graph, Tuning, true);
			const int Loops = static_cast<int>(std::lround(Tuning.LoopFraction * static_cast<double>(Graph.Nodes.size())));
			for (int n = 0; n < Loops; ++n)
			{
				int Best = -1;
				double BestScore = 0.0;
				for (size_t c = 0; c < Candidates.size(); ++c)
				{
					const Candidate& C = Candidates[c];
					if (HasPassage(Graph, C.A, C.B))
					{
						continue;
					}
					const double Swim = Dijkstra(Graph, {C.A}, -1, nullptr)[static_cast<size_t>(C.B)];
					const double Score = (Swim / std::fmax(C.Distance, 1.0)) * RangeD(R, 0.75, 1.25);
					if (Score > BestScore)
					{
						BestScore = Score;
						Best = static_cast<int>(c);
					}
				}
				if (Best < 0 || BestScore < 1.6)
				{
					break;  // nothing left that would make a meaningful detour
				}
				const Candidate& Pick = Candidates[static_cast<size_t>(Best)];
				Graph.Passages.push_back(MakePassage(R, Graph, Pick.A, Pick.B, true, Tuning));
			}

			// The arena must be the farthest swim, and must never hang off a single passage. Culling can
			// remove an alternative route and new routes can shorten the swim to the arena, so repeat
			// until neither step changes anything.
			for (int Round = 0; Round < 8; ++Round)
			{
				const size_t NodesBefore = Graph.Nodes.size();
				const size_t PassagesBefore = Graph.Passages.size();
				CullBeyondArena(Graph);
				if (!SecondRouteToArena(R, Graph, Tuning))
				{
					return Graph;
				}
				if (Graph.Nodes.size() == NodesBefore && Graph.Passages.size() == PassagesBefore)
				{
					break;
				}
			}

			if (!PlaceAir(Graph, Tuning))
			{
				return Graph;
			}

			// Dead ends become rooms for later milestones.
			std::vector<int> Degree(Graph.Nodes.size(), 0);
			for (const CavePassage& Passage : Graph.Passages)
			{
				++Degree[static_cast<size_t>(Passage.A)];
				++Degree[static_cast<size_t>(Passage.B)];
			}
			for (size_t i = 0; i < Graph.Nodes.size(); ++i)
			{
				CaveNode& Node = Graph.Nodes[i];
				if (Node.Kind == ECaveNodeKind::Chamber && Degree[i] == 1 && R.Chance(static_cast<float>(Tuning.SideRoomChance)))
				{
					Node.Kind = R.Chance(0.5f) ? ECaveNodeKind::Mineral : ECaveNodeKind::Nest;
				}
			}

			const std::vector<double> Route = Dijkstra(Graph, {Graph.Entrance}, -1, nullptr);
			const std::vector<double> Air = Dijkstra(Graph, AirNodes(Graph), -1, nullptr);
			for (size_t i = 0; i < Graph.Nodes.size(); ++i)
			{
				Graph.Nodes[i].RouteDistance = Route[i];
				Graph.Nodes[i].AirDistance = Air[i];
			}
			return Graph;
		}
	}

	const char* CaveNodeKindName(ECaveNodeKind Kind)
	{
		switch (Kind)
		{
		case ECaveNodeKind::Chamber:   return "chamber";
		case ECaveNodeKind::Entrance:  return "entrance";
		case ECaveNodeKind::AirPocket: return "air pocket";
		case ECaveNodeKind::Mineral:   return "mineral room";
		case ECaveNodeKind::Nest:      return "nest";
		case ECaveNodeKind::Arena:     return "arena";
		}
		return "unknown";
	}

	const char* CavePassageKindName(ECavePassageKind Kind)
	{
		switch (Kind)
		{
		case ECavePassageKind::Tunnel:  return "tunnel";
		case ECavePassageKind::Squeeze: return "squeeze";
		case ECavePassageKind::Shaft:   return "shaft";
		}
		return "unknown";
	}

	double CaveNode::BoundRadius() const
	{
		return std::fmax(Radii.X, std::fmax(Radii.Y, Radii.Z));
	}

	double CaveNode::FloorZ() const
	{
		return Centre.Z - Radii.Z * 0.6;
	}

	Box3 CaveGraph::Bounds() const
	{
		const double Big = std::numeric_limits<double>::max();
		Box3 Box{Vec3(Big, Big, Big), Vec3(-Big, -Big, -Big)};
		for (const CaveNode& Node : Nodes)
		{
			Box.Min = Vec3::Min(Box.Min, Node.Centre - Node.Radii);
			Box.Max = Vec3::Max(Box.Max, Node.Centre + Node.Radii);
		}
		for (const CavePassage& Passage : Passages)
		{
			const Vec3 Pad(Passage.Radius, Passage.Radius, Passage.Radius);
			for (const Vec3& P : Passage.Path)
			{
				Box.Min = Vec3::Min(Box.Min, P - Pad);
				Box.Max = Vec3::Max(Box.Max, P + Pad);
			}
		}
		return Box;
	}

	int CaveGraph::CountKind(ECaveNodeKind Kind) const
	{
		return static_cast<int>(std::count_if(Nodes.begin(), Nodes.end(), [Kind](const CaveNode& N) { return N.Kind == Kind; }));
	}

	std::vector<double> CaveRouteDistances(const CaveGraph& Graph, int Source, int SkipPassage)
	{
		std::vector<double> Dist = Dijkstra(Graph, {Source}, SkipPassage, nullptr);
		for (double& D : Dist)
		{
			if (D == Unreachable)
			{
				D = -1.0;
			}
		}
		return Dist;
	}

	std::vector<double> CaveAirDistances(const CaveGraph& Graph)
	{
		std::vector<double> Dist = Dijkstra(Graph, AirNodes(Graph), -1, nullptr);
		for (double& D : Dist)
		{
			if (D == Unreachable)
			{
				D = -1.0;
			}
		}
		return Dist;
	}

	std::vector<int> CaveRoute(const CaveGraph& Graph, int From, int To)
	{
		std::vector<int> Via;
		const std::vector<double> Dist = Dijkstra(Graph, {From}, -1, &Via);
		std::vector<int> Route;
		if (To < 0 || static_cast<size_t>(To) >= Dist.size() || Dist[static_cast<size_t>(To)] == Unreachable)
		{
			return Route;
		}
		for (int Node = To; Node != From;)
		{
			const int p = Via[static_cast<size_t>(Node)];
			Route.push_back(p);
			Node = Graph.Passages[static_cast<size_t>(p)].Other(Node);
		}
		std::reverse(Route.begin(), Route.end());
		return Route;
	}

	CaveGraph GenerateCaveGraph(uint64_t Seed, const CaveLayoutTuning& Tuning, std::vector<std::string>* AttemptLog)
	{
		CaveGraph Last;
		for (int Attempt = 0; Attempt < MaxLayoutAttempts; ++Attempt)
		{
			Rng R(MixSeed(Seed, 0xCA0E0000ULL + static_cast<uint64_t>(Attempt)));
			CaveGraph Graph = TryLayout(R, Seed, Tuning);
			Graph.Attempts = Attempt + 1;
			const CaveReport Report = CheckCaveGraph(Graph, Tuning);
			if (Report.Ok())
			{
				return Graph;
			}
			if (AttemptLog)
			{
				AttemptLog->push_back(Report.Problem + " (" + std::to_string(Graph.Nodes.size()) + " chambers, "
					+ std::to_string(Graph.Passages.size()) + " passages)");
			}
			Last = std::move(Graph);
		}
		return Last;
	}

	CaveReport CheckCaveGraph(const CaveGraph& Graph, const CaveLayoutTuning& Tuning)
	{
		CaveReport Report;
		const size_t Count = Graph.Nodes.size();
		auto Fail = [&Report](const char* Why)
		{
			if (Report.Problem.empty())
			{
				Report.Problem = Why;
			}
		};
		const bool bEnds = Graph.Entrance >= 0 && Graph.Arena >= 0 && static_cast<size_t>(Graph.Entrance) < Count
			&& static_cast<size_t>(Graph.Arena) < Count && Graph.CountKind(ECaveNodeKind::Entrance) == 1
			&& Graph.CountKind(ECaveNodeKind::Arena) == 1;
		Report.bEnoughChambers = bEnds && static_cast<int>(Count) >= Tuning.MinChambers;
		if (!Report.bEnoughChambers)
		{
			Fail(bEnds ? "too few chambers" : "missing entrance or arena");
			return Report;
		}

		const std::vector<double> Route = Dijkstra(Graph, {Graph.Entrance}, -1, nullptr);
		Report.bConnected = std::none_of(Route.begin(), Route.end(), [](double D) { return D == Unreachable; });
		if (!Report.bConnected)
		{
			Fail("some chambers cannot be reached");
		}

		Report.bTwoRoutesToArena = Report.bConnected;
		if (Report.bConnected)
		{
			for (const int p : CaveRoute(Graph, Graph.Entrance, Graph.Arena))
			{
				if (Dijkstra(Graph, {Graph.Entrance}, p, nullptr)[static_cast<size_t>(Graph.Arena)] == Unreachable)
				{
					Report.bTwoRoutesToArena = false;
					Fail("one passage cuts the entrance off from the arena");
					break;
				}
			}
		}

		Report.LongestSwimToAir = LongestSwimToAir(Graph, Dijkstra(Graph, AirNodes(Graph), -1, nullptr));
		Report.bAirInReach = Report.LongestSwimToAir <= Tuning.MaxAirSwim + 1e-6;
		if (!Report.bAirInReach)
		{
			Fail("air pockets too far apart");
		}

		const double ArenaRoute = Route[static_cast<size_t>(Graph.Arena)];
		Report.bArenaFarthest = std::all_of(Route.begin(), Route.end(), [ArenaRoute](double D) { return D <= ArenaRoute + 1e-6; });
		if (!Report.bArenaFarthest)
		{
			Fail("a chamber is farther from the entrance than the arena");
		}

		Report.bChambersApart = true;
		for (size_t i = 0; i < Count && Report.bChambersApart; ++i)
		{
			for (size_t j = i + 1; j < Count; ++j)
			{
				const CaveNode& A = Graph.Nodes[i];
				const CaveNode& B = Graph.Nodes[j];
				if ((A.Centre - B.Centre).Length() < A.BoundRadius() + B.BoundRadius() + Tuning.ChamberGap * 0.999)
				{
					Report.bChambersApart = false;
					Fail("two chambers overlap");
					break;
				}
			}
		}

		Report.bPassagesClear = true;
		for (const CavePassage& Passage : Graph.Passages)
		{
			const bool bValidEnds = Passage.A >= 0 && Passage.B >= 0 && static_cast<size_t>(Passage.A) < Count
				&& static_cast<size_t>(Passage.B) < Count && Passage.A != Passage.B && Passage.Path.size() >= 2;
			if (!bValidEnds || !PathClear(Graph, Passage, CheckedClearance))
			{
				Report.bPassagesClear = false;
				Fail(bValidEnds ? "a passage runs through another chamber" : "a passage is malformed");
				break;
			}
		}
		return Report;
	}
}
