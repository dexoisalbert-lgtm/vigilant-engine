#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "PaleotrenchCore/Api.h"
#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/Vec3.h"

namespace pt
{
	// 3D navigation for swimming creatures (GDD 4: NavMesh cannot handle volumes of water). A sparse grid
	// of cells that are open water with room for a body of the given clearance, searched with A* over the
	// 26 neighbours (diagonal moves may not cut rock corners), then shortened by line-of-sight checks.
	struct CaveNavSettings
	{
		double CellSize = 200.0;       // cm
		double Clearance = 60.0;       // cm of open water required around a cell's centre
		size_t MaxExpansions = 500000; // search budget per path
	};

	class CaveNav
	{
	public:
		PT_API void Build(const CaveField& Field, const CaveNavSettings& InSettings);

		// A swimmable path From -> To (both snapped to the nearest open cell within two cells), smoothed.
		// The first and last points are From and To themselves. False if there is none.
		PT_API bool FindPath(const Vec3& From, const Vec3& To, std::vector<Vec3>& OutPath) const;
		// True if every point of the segment lies in open cells.
		PT_API bool ClearLine(const Vec3& A, const Vec3& B) const;
		PT_API bool IsOpen(const Vec3& P) const;
		// Centre of the nearest open cell within two cells of P.
		PT_API bool NearestOpen(const Vec3& P, Vec3& Out) const;

		size_t OpenCellCount() const { return Cells.size(); }
		double CellSize() const { return Settings.CellSize; }

	private:
		struct Cell
		{
			int32_t X = 0;
			int32_t Y = 0;
			int32_t Z = 0;
		};

		static uint64_t Pack(int32_t X, int32_t Y, int32_t Z);
		int32_t Find(int32_t X, int32_t Y, int32_t Z) const;
		int32_t FindNearest(const Vec3& P) const;
		Vec3 Centre(const Cell& C) const;

		CaveNavSettings Settings;
		std::vector<Cell> Cells;
		std::unordered_map<uint64_t, int32_t> Lookup;
	};
}
