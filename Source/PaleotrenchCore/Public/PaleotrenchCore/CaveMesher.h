#pragma once

#include <cstdint>
#include <vector>

#include "PaleotrenchCore/Api.h"
#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/Vec3.h"

namespace pt
{
	// Step 3 of cave generation: the rock surface as triangle meshes, cut into cubic chunks so they can
	// be built on worker threads and streamed in and out around the player. Surface nets: one vertex
	// per voxel the surface passes through, one quad per voxel edge it crosses. A chunk owns the edges
	// that start inside it, and neighbouring chunks compute their shared border vertices from identical
	// samples, so chunks meet without cracks.
	struct CaveMeshSettings
	{
		double VoxelSize = 50.0;  // cm
		int ChunkCells = 32;      // voxels along each side of a chunk

		double ChunkSize() const { return VoxelSize * static_cast<double>(ChunkCells); }
	};

	struct CaveChunkKey
	{
		int X = 0;
		int Y = 0;
		int Z = 0;

		bool operator==(const CaveChunkKey& O) const { return X == O.X && Y == O.Y && Z == O.Z; }
		bool operator!=(const CaveChunkKey& O) const { return !(*this == O); }
		bool operator<(const CaveChunkKey& O) const
		{
			return X != O.X ? X < O.X : (Y != O.Y ? Y < O.Y : Z < O.Z);
		}
	};

	struct CaveChunkMesh
	{
		std::vector<float> Positions;   // x, y, z per vertex: cm in cave space
		std::vector<float> Normals;     // unit, pointing out of the rock into the open water
		std::vector<uint32_t> Indices;  // three per triangle, counter-clockwise seen from the open side
		                                // in a right-handed frame (flip for a left-handed renderer)

		size_t VertexCount() const { return Positions.size() / 3; }
		size_t TriangleCount() const { return Indices.size() / 3; }
		bool Empty() const { return Indices.empty(); }
		void Clear()
		{
			Positions.clear();
			Normals.clear();
			Indices.clear();
		}
	};

	PT_API CaveChunkKey CaveChunkAt(const Vec3& P, const CaveMeshSettings& Settings);
	PT_API Box3 CaveChunkBox(const CaveChunkKey& Key, const CaveMeshSettings& Settings);
	// Every chunk that rock surface can pass through (some may still turn out empty).
	PT_API std::vector<CaveChunkKey> CaveChunksToMesh(const CaveField& Field, const CaveMeshSettings& Settings);
	// Builds one chunk. Thread-safe: only reads the field.
	PT_API void MeshCaveChunk(const CaveField& Field, const CaveChunkKey& Key, const CaveMeshSettings& Settings, CaveChunkMesh& Out);
}
