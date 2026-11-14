#include "TestFramework.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/CaveMesher.h"
#include "PaleotrenchCore/CaveNav.h"
#include "PaleotrenchCore/Seed.h"

namespace
{
	const pt::CaveGraph& TestGraph()
	{
		static const pt::CaveGraph Graph = pt::GenerateCaveGraph(pt::HashText("geometry tests"), pt::CaveLayoutTuning());
		return Graph;
	}

	const pt::CaveField& TestField()
	{
		static const pt::CaveField Field(TestGraph());
		return Field;
	}

	// A chunk the rock surface passes through near the given point.
	pt::CaveChunkKey SurfaceChunkNear(const pt::Vec3& P, const pt::CaveMeshSettings& Settings)
	{
		pt::CaveChunkKey Key = pt::CaveChunkAt(P, Settings);
		pt::CaveChunkMesh Mesh;
		for (int Step = 0; Step < 64; ++Step)
		{
			pt::MeshCaveChunk(TestField(), Key, Settings, Mesh);
			if (!Mesh.Empty())
			{
				return Key;
			}
			Key.Z += 1;  // chambers are open in the middle: walk up to the ceiling
		}
		return Key;
	}

	using PointKey = std::tuple<float, float, float>;
}

PT_TEST(CaveField_ChambersPassagesAndDomesAreOpen)
{
	const pt::CaveGraph& Graph = TestGraph();
	const pt::CaveField& Field = TestField();
	for (const pt::CaveNode& Node : Graph.Nodes)
	{
		PT_EXPECT(Field.Sample(Node.Centre) < 0.0);
	}
	for (const pt::CavePassage& Passage : Graph.Passages)
	{
		// Noise may never close a passage: its whole centreline stays open with room to spare.
		for (const pt::Vec3& P : Passage.Path)
		{
			PT_EXPECT(Field.Sample(P) < -Passage.Radius * 0.3);
		}
	}
	PT_EXPECT(Field.AirDomes().size() == static_cast<size_t>(Graph.CountKind(pt::ECaveNodeKind::Entrance) + Graph.CountKind(pt::ECaveNodeKind::AirPocket)));
	for (const pt::CaveAirDome& Dome : Field.AirDomes())
	{
		// The air pocket actor's 4 x 4 x 2.5 m box must sit in open space: centre of its water surface,
		// its corners, and the top of the air.
		PT_EXPECT(Field.Sample(Dome.WaterCentre) < 0.0);
		PT_EXPECT(Field.Sample(Dome.WaterCentre + pt::Vec3(0.0, 0.0, 240.0)) < 0.0);
		PT_EXPECT(Field.Sample(Dome.WaterCentre + pt::Vec3(190.0, 190.0, 0.0)) < 0.0);
		PT_EXPECT(Field.Sample(Dome.WaterCentre + pt::Vec3(-190.0, 190.0, 0.0)) < 0.0);
		// The dome rises above the chamber's own ceiling.
		const pt::CaveNode& Node = Graph.Nodes[static_cast<size_t>(Dome.Node)];
		PT_EXPECT(Dome.WaterCentre.Z + Dome.Height > Node.CeilingZ());
	}
	// Far outside every chamber and passage is solid rock.
	const pt::Box3& Bounds = Field.Bounds();
	PT_EXPECT(Field.Sample(Bounds.Max + pt::Vec3(5000.0, 5000.0, 5000.0)) > 0.0);
	PT_EXPECT(Field.Sample(pt::Vec3(Bounds.Min.X - 3000.0, 0.0, 0.0)) > 0.0);
}

PT_TEST(CaveField_ChunkFeatureListsGiveTheSameSurface)
{
	// Sampling with a chunk's local feature list must match sampling with every feature near the surface.
	const pt::CaveField& Field = TestField();
	const pt::CaveGraph& Graph = TestGraph();
	int Checked = 0;
	int Mismatch = 0;
	for (const pt::CavePassage& Passage : Graph.Passages)
	{
		const pt::Vec3 Mid = Passage.Path[Passage.Path.size() / 2];
		for (int k = 0; k < 8; ++k)
		{
			const pt::Vec3 P = Mid + pt::Vec3(Passage.Radius * std::cos(k * 0.785), Passage.Radius * std::sin(k * 0.785), Passage.Radius * 0.3);
			const pt::Box3 Local{P - pt::Vec3(800.0, 800.0, 800.0), P + pt::Vec3(800.0, 800.0, 800.0)};
			const double Near = Field.SampleWith(P, Field.FeaturesNear(Local));
			const double All = Field.Sample(P);
			++Checked;
			Mismatch += std::fabs(Near - All) < 1e-9 ? 0 : 1;
		}
	}
	PT_EXPECT(Checked > 50);
	PT_EXPECT(Mismatch == 0);
}

PT_TEST(CaveMesher_ChunksAreDeterministicAndFaceTheWater)
{
	const pt::CaveMeshSettings Settings;
	const pt::CaveChunkKey Key = SurfaceChunkNear(TestGraph().Nodes[static_cast<size_t>(TestGraph().Entrance)].Centre, Settings);
	pt::CaveChunkMesh A;
	pt::CaveChunkMesh B;
	pt::MeshCaveChunk(TestField(), Key, Settings, A);
	pt::MeshCaveChunk(TestField(), Key, Settings, B);
	PT_EXPECT(!A.Empty());
	PT_EXPECT(A.Positions == B.Positions);
	PT_EXPECT(A.Indices == B.Indices);
	PT_EXPECT(A.Normals.size() == A.Positions.size());
	PT_EXPECT(A.Indices.size() % 3 == 0);
	// Every triangle's winding agrees with the smooth normals (which point into the water).
	int Agree = 0;
	int Total = 0;
	std::map<std::pair<uint32_t, uint32_t>, int> EdgeUse;
	for (size_t t = 0; t < A.Indices.size(); t += 3)
	{
		pt::Vec3 P[3];
		pt::Vec3 N;
		for (int c = 0; c < 3; ++c)
		{
			const size_t v = static_cast<size_t>(A.Indices[t + static_cast<size_t>(c)]) * 3;
			PT_EXPECT(v + 2 < A.Positions.size());
			P[c] = pt::Vec3(A.Positions[v], A.Positions[v + 1], A.Positions[v + 2]);
			N += pt::Vec3(A.Normals[v], A.Normals[v + 1], A.Normals[v + 2]);
			const uint32_t E0 = A.Indices[t + static_cast<size_t>(c)];
			const uint32_t E1 = A.Indices[t + static_cast<size_t>((c + 1) % 3)];
			++EdgeUse[{std::min(E0, E1), std::max(E0, E1)}];
		}
		Agree += (P[1] - P[0]).Cross(P[2] - P[0]).Dot(N) > 0.0 ? 1 : 0;
		++Total;
	}
	PT_EXPECT(Agree >= Total * 99 / 100);
	// No edge is shared by more than two triangles.
	int Overused = 0;
	for (const auto& [Edge, Uses] : EdgeUse)
	{
		Overused += Uses > 2 ? 1 : 0;
	}
	PT_EXPECT(Overused == 0);
	for (const float Value : A.Normals)
	{
		PT_EXPECT(std::isfinite(Value));
	}
}

PT_TEST(CaveMesher_NeighbouringChunksShareTheirBorder)
{
	const pt::CaveMeshSettings Settings;
	const pt::CaveField& Field = TestField();
	// Find a chunk with surface whose +X neighbour also has surface.
	int Pairs = 0;
	int Shared = 0;
	int Missing = 0;
	for (const pt::CaveChunkKey& Key : pt::CaveChunksToMesh(Field, Settings))
	{
		if (Pairs >= 6)
		{
			break;
		}
		pt::CaveChunkMesh A;
		pt::CaveChunkMesh B;
		pt::MeshCaveChunk(Field, Key, Settings, A);
		if (A.Empty())
		{
			continue;
		}
		pt::MeshCaveChunk(Field, pt::CaveChunkKey{Key.X + 1, Key.Y, Key.Z}, Settings, B);
		if (B.Empty())
		{
			continue;
		}
		++Pairs;
		const double Border = pt::CaveChunkBox(pt::CaveChunkKey{Key.X + 1, Key.Y, Key.Z}, Settings).Min.X;
		std::set<PointKey> InA;
		for (size_t v = 0; v < A.Positions.size(); v += 3)
		{
			InA.insert({A.Positions[v], A.Positions[v + 1], A.Positions[v + 2]});
		}
		// B's vertices in the voxel layer it shares with A must exist, bit for bit, in A.
		for (size_t v = 0; v < B.Positions.size(); v += 3)
		{
			if (B.Positions[v] < Border)
			{
				const bool bFound = InA.count({B.Positions[v], B.Positions[v + 1], B.Positions[v + 2]}) > 0;
				Shared += bFound ? 1 : 0;
				Missing += bFound ? 0 : 1;
			}
		}
	}
	std::printf("    %d chunk pairs: %d shared border vertices, %d without a twin\n", Pairs, Shared, Missing);
	PT_EXPECT(Pairs > 0);
	PT_EXPECT(Shared > 0);
	PT_EXPECT(Missing <= Shared / 20);
}

PT_TEST(CaveMesher_WholeCaveBuildsInReasonableTime)
{
	const pt::CaveMeshSettings Settings;
	const std::vector<pt::CaveChunkKey> Keys = pt::CaveChunksToMesh(TestField(), Settings);
	const auto Start = std::chrono::steady_clock::now();
	size_t Triangles = 0;
	size_t WithSurface = 0;
	pt::CaveChunkMesh Mesh;
	for (const pt::CaveChunkKey& Key : Keys)
	{
		pt::MeshCaveChunk(TestField(), Key, Settings, Mesh);
		Triangles += Mesh.TriangleCount();
		WithSurface += Mesh.Empty() ? 0 : 1;
	}
	const double Seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	std::printf("    %zu candidate chunks, %zu with rock surface, %zu triangles, %.1f s on one thread (%.0f ms per chunk)\n",
		Keys.size(), WithSurface, Triangles, Seconds, Seconds * 1000.0 / static_cast<double>(std::max<size_t>(1, Keys.size())));
	PT_EXPECT(WithSurface > 20);
	PT_EXPECT(Triangles > 10000);
}

PT_TEST(CaveNav_FindsASwimmablePathToTheArena)
{
	const pt::CaveGraph& Graph = TestGraph();
	pt::CaveNav Nav;
	const auto Start = std::chrono::steady_clock::now();
	Nav.Build(TestField(), pt::CaveNavSettings());
	const double BuildSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
	const pt::Vec3 From = Graph.Nodes[static_cast<size_t>(Graph.Entrance)].Centre;
	const pt::Vec3 To = Graph.Nodes[static_cast<size_t>(Graph.Arena)].Centre;
	std::vector<pt::Vec3> Path;
	const auto PathStart = std::chrono::steady_clock::now();
	const bool bFound = Nav.FindPath(From, To, Path);
	const double PathSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - PathStart).count();
	double Length = 0.0;
	for (size_t i = 0; i + 1 < Path.size(); ++i)
	{
		Length += (Path[i + 1] - Path[i]).Length();
		PT_EXPECT(Nav.ClearLine(Path[i], Path[i + 1]));
	}
	std::printf("    %zu open cells built in %.2f s; entrance -> arena: %zu waypoints, %.0f m, found in %.0f ms\n", Nav.OpenCellCount(),
		BuildSeconds, Path.size(), Length / 100.0, PathSeconds * 1000.0);
	PT_EXPECT(Nav.OpenCellCount() > 1000);
	PT_EXPECT(bFound);
	PT_EXPECT(Path.size() >= 2);
	PT_EXPECT(Length >= (To - From).Length() - 1.0);
	// The swim can't be shorter than the straight line, and the passages make it longer.
	PT_EXPECT(Length <= Graph.Nodes[static_cast<size_t>(Graph.Arena)].RouteDistance * 1.5);
	// Deep in the rock there is nowhere to go.
	std::vector<pt::Vec3> None;
	PT_EXPECT(!Nav.FindPath(TestField().Bounds().Max + pt::Vec3(10000.0, 0.0, 0.0), To, None));
}
