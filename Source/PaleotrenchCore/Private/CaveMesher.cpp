#include "PaleotrenchCore/CaveMesher.h"

#include <cmath>
#include <cstddef>

namespace pt
{
	namespace
	{
		int FloorDiv(double Value, double Size)
		{
			return static_cast<int>(std::floor(Value / Size));
		}

		// Sample position for grid index I of chunk coordinate C: computed from integers first so two
		// chunks sharing a border sample get bit-identical positions.
		double GridCoord(int Chunk, int Cells, int I, double Voxel)
		{
			return static_cast<double>(static_cast<int64_t>(Chunk) * Cells + I) * Voxel;
		}

		// The 12 edges of a voxel as pairs of corner numbers (corner bit 0 = +x, bit 1 = +y, bit 2 = +z).
		constexpr int CubeEdges[12][2] = {
			{0, 1}, {2, 3}, {4, 5}, {6, 7},  // along x
			{0, 2}, {1, 3}, {4, 6}, {5, 7},  // along y
			{0, 4}, {1, 5}, {2, 6}, {3, 7},  // along z
		};
	}

	CaveChunkKey CaveChunkAt(const Vec3& P, const CaveMeshSettings& Settings)
	{
		const double Size = Settings.ChunkSize();
		return CaveChunkKey{FloorDiv(P.X, Size), FloorDiv(P.Y, Size), FloorDiv(P.Z, Size)};
	}

	Box3 CaveChunkBox(const CaveChunkKey& Key, const CaveMeshSettings& Settings)
	{
		const int N = Settings.ChunkCells;
		const double V = Settings.VoxelSize;
		return Box3{Vec3(GridCoord(Key.X, N, 0, V), GridCoord(Key.Y, N, 0, V), GridCoord(Key.Z, N, 0, V)),
			Vec3(GridCoord(Key.X, N, N, V), GridCoord(Key.Y, N, N, V), GridCoord(Key.Z, N, N, V))};
	}

	std::vector<CaveChunkKey> CaveChunksToMesh(const CaveField& Field, const CaveMeshSettings& Settings)
	{
		std::vector<CaveChunkKey> Keys;
		const CaveChunkKey Lo = CaveChunkAt(Field.Bounds().Min, Settings);
		const CaveChunkKey Hi = CaveChunkAt(Field.Bounds().Max, Settings);
		for (int z = Lo.Z; z <= Hi.Z; ++z)
		{
			for (int y = Lo.Y; y <= Hi.Y; ++y)
			{
				for (int x = Lo.X; x <= Hi.X; ++x)
				{
					const CaveChunkKey Key{x, y, z};
					if (!Field.FeaturesNear(CaveChunkBox(Key, Settings).Expanded(Settings.VoxelSize * 2.0)).empty())
					{
						Keys.push_back(Key);
					}
				}
			}
		}
		return Keys;
	}

	void MeshCaveChunk(const CaveField& Field, const CaveChunkKey& Key, const CaveMeshSettings& Settings, CaveChunkMesh& Out)
	{
		Out.Clear();
		const int N = Settings.ChunkCells;
		const double V = Settings.VoxelSize;
		const int S = N + 2;  // samples per axis: grid indices -1 .. N
		const Box3 SampleBox = CaveChunkBox(Key, Settings).Expanded(V);
		const std::vector<int> Nearby = Field.FeaturesNear(SampleBox.Expanded(V));
		if (Nearby.empty())
		{
			return;
		}
		const auto SampleIndex = [S](int X, int Y, int Z)
		{
			return (static_cast<size_t>(Z + 1) * static_cast<size_t>(S) + static_cast<size_t>(Y + 1)) * static_cast<size_t>(S)
				+ static_cast<size_t>(X + 1);
		};
		const auto Position = [&Key, N, V](int X, int Y, int Z)
		{
			return Vec3(GridCoord(Key.X, N, X, V), GridCoord(Key.Y, N, Y, V), GridCoord(Key.Z, N, Z, V));
		};

		std::vector<double> Values(static_cast<size_t>(S) * static_cast<size_t>(S) * static_cast<size_t>(S));
		bool bAnyOpen = false;
		bool bAnyRock = false;
		for (int z = -1; z <= N; ++z)
		{
			for (int y = -1; y <= N; ++y)
			{
				for (int x = -1; x <= N; ++x)
				{
					const double Value = Field.SampleWith(Position(x, y, z), Nearby);
					Values[SampleIndex(x, y, z)] = Value;
					bAnyOpen = bAnyOpen || Value < 0.0;
					bAnyRock = bAnyRock || Value >= 0.0;
				}
			}
		}
		if (!bAnyOpen || !bAnyRock)
		{
			return;  // solid rock or open water throughout
		}

		// One vertex per voxel the surface passes through (voxels -1 .. N-1 along each axis), placed at the
		// average of the points where the surface crosses the voxel's edges.
		const int C = N + 1;
		const auto CellIndex = [C](int X, int Y, int Z)
		{
			return (static_cast<size_t>(Z + 1) * static_cast<size_t>(C) + static_cast<size_t>(Y + 1)) * static_cast<size_t>(C)
				+ static_cast<size_t>(X + 1);
		};
		std::vector<int32_t> CellVertex(static_cast<size_t>(C) * static_cast<size_t>(C) * static_cast<size_t>(C), -1);
		std::vector<Vec3> CellPoints;
		std::vector<int32_t> Emitted;  // output index for each cell point, -1 until used
		for (int z = -1; z < N; ++z)
		{
			for (int y = -1; y < N; ++y)
			{
				for (int x = -1; x < N; ++x)
				{
					double Corner[8];
					int Open = 0;
					for (int c = 0; c < 8; ++c)
					{
						Corner[c] = Values[SampleIndex(x + (c & 1), y + ((c >> 1) & 1), z + ((c >> 2) & 1))];
						Open += Corner[c] < 0.0 ? 1 : 0;
					}
					if (Open == 0 || Open == 8)
					{
						continue;
					}
					Vec3 Sum;
					int Crossings = 0;
					for (const auto& Edge : CubeEdges)
					{
						const double A = Corner[Edge[0]];
						const double B = Corner[Edge[1]];
						if ((A < 0.0) == (B < 0.0))
						{
							continue;
						}
						const double T = A / (A - B);
						const Vec3 Pa = Position(x + (Edge[0] & 1), y + ((Edge[0] >> 1) & 1), z + ((Edge[0] >> 2) & 1));
						const Vec3 Pb = Position(x + (Edge[1] & 1), y + ((Edge[1] >> 1) & 1), z + ((Edge[1] >> 2) & 1));
						Sum += Vec3::Lerp(Pa, Pb, T);
						++Crossings;
					}
					CellVertex[CellIndex(x, y, z)] = static_cast<int32_t>(CellPoints.size());
					CellPoints.push_back(Sum / static_cast<double>(Crossings));
				}
			}
		}
		Emitted.assign(CellPoints.size(), -1);

		const auto Emit = [&](int32_t Point) -> uint32_t
		{
			int32_t& Slot = Emitted[static_cast<size_t>(Point)];
			if (Slot < 0)
			{
				Slot = static_cast<int32_t>(Out.Positions.size() / 3);
				const Vec3& P = CellPoints[static_cast<size_t>(Point)];
				Out.Positions.push_back(static_cast<float>(P.X));
				Out.Positions.push_back(static_cast<float>(P.Y));
				Out.Positions.push_back(static_cast<float>(P.Z));
				const Vec3 Normal = (-Field.Gradient(P, Nearby, V * 0.5)).Normalized();
				Out.Normals.push_back(static_cast<float>(Normal.X));
				Out.Normals.push_back(static_cast<float>(Normal.Y));
				Out.Normals.push_back(static_cast<float>(Normal.Z));
			}
			return static_cast<uint32_t>(Slot);
		};
		const auto Triangle = [&](int32_t A, int32_t B, int32_t D, const Vec3& Facing)
		{
			const Vec3& Pa = CellPoints[static_cast<size_t>(A)];
			const Vec3& Pb = CellPoints[static_cast<size_t>(B)];
			const Vec3& Pd = CellPoints[static_cast<size_t>(D)];
			const Vec3 Normal = (Pb - Pa).Cross(Pd - Pa);
			if (Normal.LengthSq() < 1e-12)
			{
				return;  // degenerate
			}
			const bool bFlip = Normal.Dot(Facing) < 0.0;
			Out.Indices.push_back(Emit(A));
			Out.Indices.push_back(Emit(bFlip ? D : B));
			Out.Indices.push_back(Emit(bFlip ? B : D));
		};

		// One quad per crossed voxel edge that starts inside this chunk, joining the four voxels around it.
		for (int z = 0; z < N; ++z)
		{
			for (int y = 0; y < N; ++y)
			{
				for (int x = 0; x < N; ++x)
				{
					const double Here = Values[SampleIndex(x, y, z)];
					for (int Axis = 0; Axis < 3; ++Axis)
					{
						const int Nx = x + (Axis == 0 ? 1 : 0);
						const int Ny = y + (Axis == 1 ? 1 : 0);
						const int Nz = z + (Axis == 2 ? 1 : 0);
						const double There = Values[SampleIndex(Nx, Ny, Nz)];
						if ((Here < 0.0) == (There < 0.0))
						{
							continue;
						}
						int32_t Q[4];
						if (Axis == 0)
						{
							Q[0] = CellVertex[CellIndex(x, y - 1, z - 1)];
							Q[1] = CellVertex[CellIndex(x, y, z - 1)];
							Q[2] = CellVertex[CellIndex(x, y, z)];
							Q[3] = CellVertex[CellIndex(x, y - 1, z)];
						}
						else if (Axis == 1)
						{
							Q[0] = CellVertex[CellIndex(x - 1, y, z - 1)];
							Q[1] = CellVertex[CellIndex(x, y, z - 1)];
							Q[2] = CellVertex[CellIndex(x, y, z)];
							Q[3] = CellVertex[CellIndex(x - 1, y, z)];
						}
						else
						{
							Q[0] = CellVertex[CellIndex(x - 1, y - 1, z)];
							Q[1] = CellVertex[CellIndex(x, y - 1, z)];
							Q[2] = CellVertex[CellIndex(x, y, z)];
							Q[3] = CellVertex[CellIndex(x - 1, y, z)];
						}
						if (Q[0] < 0 || Q[1] < 0 || Q[2] < 0 || Q[3] < 0)
						{
							continue;
						}
						// The open side is where the field is negative.
						const double Sign = Here < 0.0 ? -1.0 : 1.0;
						const Vec3 Facing(Axis == 0 ? Sign : 0.0, Axis == 1 ? Sign : 0.0, Axis == 2 ? Sign : 0.0);
						const double D02 = (CellPoints[static_cast<size_t>(Q[0])] - CellPoints[static_cast<size_t>(Q[2])]).LengthSq();
						const double D13 = (CellPoints[static_cast<size_t>(Q[1])] - CellPoints[static_cast<size_t>(Q[3])]).LengthSq();
						if (D02 <= D13)
						{
							Triangle(Q[0], Q[1], Q[2], Facing);
							Triangle(Q[0], Q[2], Q[3], Facing);
						}
						else
						{
							Triangle(Q[1], Q[2], Q[3], Facing);
							Triangle(Q[1], Q[3], Q[0], Facing);
						}
					}
				}
			}
		}
	}
}
