#include "PaleotrenchCore/CaveNav.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>

namespace pt
{
	namespace
	{
		constexpr int32_t PackBias = 1 << 20;
		constexpr double BlockSize = 1600.0;  // cm: nav cells are tested block by block with one feature list each
	}

	uint64_t CaveNav::Pack(int32_t X, int32_t Y, int32_t Z)
	{
		const uint64_t Ux = static_cast<uint64_t>(static_cast<int64_t>(X) + PackBias) & 0x1FFFFFULL;
		const uint64_t Uy = static_cast<uint64_t>(static_cast<int64_t>(Y) + PackBias) & 0x1FFFFFULL;
		const uint64_t Uz = static_cast<uint64_t>(static_cast<int64_t>(Z) + PackBias) & 0x1FFFFFULL;
		return Ux | (Uy << 21u) | (Uz << 42u);
	}

	int32_t CaveNav::Find(int32_t X, int32_t Y, int32_t Z) const
	{
		const auto It = Lookup.find(Pack(X, Y, Z));
		return It == Lookup.end() ? -1 : It->second;
	}

	Vec3 CaveNav::Centre(const Cell& C) const
	{
		return Vec3((static_cast<double>(C.X) + 0.5) * Settings.CellSize, (static_cast<double>(C.Y) + 0.5) * Settings.CellSize,
			(static_cast<double>(C.Z) + 0.5) * Settings.CellSize);
	}

	void CaveNav::Build(const CaveField& Field, const CaveNavSettings& InSettings)
	{
		Settings = InSettings;
		Cells.clear();
		Lookup.clear();
		const double Size = Settings.CellSize;
		const Box3& Bounds = Field.Bounds();
		const int32_t Bx0 = static_cast<int32_t>(std::floor(Bounds.Min.X / BlockSize));
		const int32_t By0 = static_cast<int32_t>(std::floor(Bounds.Min.Y / BlockSize));
		const int32_t Bz0 = static_cast<int32_t>(std::floor(Bounds.Min.Z / BlockSize));
		const int32_t Bx1 = static_cast<int32_t>(std::floor(Bounds.Max.X / BlockSize));
		const int32_t By1 = static_cast<int32_t>(std::floor(Bounds.Max.Y / BlockSize));
		const int32_t Bz1 = static_cast<int32_t>(std::floor(Bounds.Max.Z / BlockSize));
		for (int32_t bz = Bz0; bz <= Bz1; ++bz)
		{
			for (int32_t by = By0; by <= By1; ++by)
			{
				for (int32_t bx = Bx0; bx <= Bx1; ++bx)
				{
					const Box3 Block{Vec3(bx * BlockSize, by * BlockSize, bz * BlockSize),
						Vec3((bx + 1) * BlockSize, (by + 1) * BlockSize, (bz + 1) * BlockSize)};
					const std::vector<int> Nearby = Field.FeaturesNear(Block.Expanded(Size));
					if (Nearby.empty())
					{
						continue;
					}
					const int32_t Cx0 = static_cast<int32_t>(std::floor(Block.Min.X / Size));
					const int32_t Cy0 = static_cast<int32_t>(std::floor(Block.Min.Y / Size));
					const int32_t Cz0 = static_cast<int32_t>(std::floor(Block.Min.Z / Size));
					const int32_t Cx1 = static_cast<int32_t>(std::ceil(Block.Max.X / Size));
					const int32_t Cy1 = static_cast<int32_t>(std::ceil(Block.Max.Y / Size));
					const int32_t Cz1 = static_cast<int32_t>(std::ceil(Block.Max.Z / Size));
					for (int32_t z = Cz0; z < Cz1; ++z)
					{
						for (int32_t y = Cy0; y < Cy1; ++y)
						{
							for (int32_t x = Cx0; x < Cx1; ++x)
							{
								const Cell C{x, y, z};
								const Vec3 P = Centre(C);
								// Each cell belongs to the block holding its centre, so no cell is tested twice.
								if (!Block.Contains(P) || P.X == Block.Max.X || P.Y == Block.Max.Y || P.Z == Block.Max.Z)
								{
									continue;
								}
								if (Field.SampleWith(P, Nearby) < -Settings.Clearance)
								{
									Lookup.emplace(Pack(x, y, z), static_cast<int32_t>(Cells.size()));
									Cells.push_back(C);
								}
							}
						}
					}
				}
			}
		}
	}

	bool CaveNav::IsOpen(const Vec3& P) const
	{
		const double Size = Settings.CellSize;
		return Find(static_cast<int32_t>(std::floor(P.X / Size)), static_cast<int32_t>(std::floor(P.Y / Size)),
			static_cast<int32_t>(std::floor(P.Z / Size))) >= 0;
	}

	int32_t CaveNav::FindNearest(const Vec3& P) const
	{
		const double Size = Settings.CellSize;
		const int32_t X = static_cast<int32_t>(std::floor(P.X / Size));
		const int32_t Y = static_cast<int32_t>(std::floor(P.Y / Size));
		const int32_t Z = static_cast<int32_t>(std::floor(P.Z / Size));
		int32_t Best = -1;
		double BestDist = std::numeric_limits<double>::max();
		for (int32_t dz = -2; dz <= 2; ++dz)
		{
			for (int32_t dy = -2; dy <= 2; ++dy)
			{
				for (int32_t dx = -2; dx <= 2; ++dx)
				{
					const int32_t Index = Find(X + dx, Y + dy, Z + dz);
					if (Index < 0)
					{
						continue;
					}
					const double Dist = (Centre(Cells[static_cast<size_t>(Index)]) - P).LengthSq();
					if (Dist < BestDist)
					{
						BestDist = Dist;
						Best = Index;
					}
				}
			}
		}
		return Best;
	}

	bool CaveNav::NearestOpen(const Vec3& P, Vec3& Out) const
	{
		const int32_t Index = FindNearest(P);
		if (Index < 0)
		{
			return false;
		}
		Out = Centre(Cells[static_cast<size_t>(Index)]);
		return true;
	}

	bool CaveNav::ClearLine(const Vec3& A, const Vec3& B) const
	{
		const double Len = (B - A).Length();
		const int Steps = std::max(1, static_cast<int>(std::ceil(Len / (Settings.CellSize * 0.25))));
		for (int s = 0; s <= Steps; ++s)
		{
			if (!IsOpen(Vec3::Lerp(A, B, static_cast<double>(s) / static_cast<double>(Steps))))
			{
				return false;
			}
		}
		return true;
	}

	bool CaveNav::FindPath(const Vec3& From, const Vec3& To, std::vector<Vec3>& OutPath) const
	{
		OutPath.clear();
		const int32_t Start = FindNearest(From);
		const int32_t Goal = FindNearest(To);
		if (Start < 0 || Goal < 0)
		{
			return false;
		}
		const size_t Count = Cells.size();
		std::vector<double> Cost(Count, std::numeric_limits<double>::max());
		std::vector<int32_t> CameFrom(Count, -1);
		using Entry = std::pair<double, int32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> Open;
		const Vec3 GoalCentre = Centre(Cells[static_cast<size_t>(Goal)]);
		Cost[static_cast<size_t>(Start)] = 0.0;
		Open.push({(Centre(Cells[static_cast<size_t>(Start)]) - GoalCentre).Length(), Start});
		size_t Expanded = 0;
		bool bFound = false;
		while (!Open.empty() && Expanded < Settings.MaxExpansions)
		{
			const int32_t Current = Open.top().second;
			Open.pop();
			if (Current == Goal)
			{
				bFound = true;
				break;
			}
			++Expanded;
			const Cell& Here = Cells[static_cast<size_t>(Current)];
			const double HereCost = Cost[static_cast<size_t>(Current)];
			for (int32_t dz = -1; dz <= 1; ++dz)
			{
				for (int32_t dy = -1; dy <= 1; ++dy)
				{
					for (int32_t dx = -1; dx <= 1; ++dx)
					{
						if (dx == 0 && dy == 0 && dz == 0)
						{
							continue;
						}
						const int32_t Next = Find(Here.X + dx, Here.Y + dy, Here.Z + dz);
						if (Next < 0)
						{
							continue;
						}
						// No cutting rock corners: every axis step on its own must also be open water.
						if ((dx != 0 && Find(Here.X + dx, Here.Y, Here.Z) < 0) || (dy != 0 && Find(Here.X, Here.Y + dy, Here.Z) < 0)
							|| (dz != 0 && Find(Here.X, Here.Y, Here.Z + dz) < 0))
						{
							continue;
						}
						const double Step = std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)) * Settings.CellSize;
						const double NewCost = HereCost + Step;
						if (NewCost < Cost[static_cast<size_t>(Next)])
						{
							Cost[static_cast<size_t>(Next)] = NewCost;
							CameFrom[static_cast<size_t>(Next)] = Current;
							Open.push({NewCost + (Centre(Cells[static_cast<size_t>(Next)]) - GoalCentre).Length(), Next});
						}
					}
				}
			}
		}
		if (!bFound)
		{
			return false;
		}
		std::vector<Vec3> Raw;
		for (int32_t Index = Goal; Index >= 0; Index = CameFrom[static_cast<size_t>(Index)])
		{
			Raw.push_back(Centre(Cells[static_cast<size_t>(Index)]));
			if (Index == Start)
			{
				break;
			}
		}
		std::reverse(Raw.begin(), Raw.end());
		Raw.front() = IsOpen(From) ? From : Raw.front();
		Raw.back() = IsOpen(To) ? To : Raw.back();
		// String pulling: from each kept point walk on while the next point is still in plain view.
		OutPath.push_back(Raw.front());
		size_t Anchor = 0;
		while (Anchor + 1 < Raw.size())
		{
			size_t Next = Anchor + 1;
			while (Next + 1 < Raw.size() && ClearLine(Raw[Anchor], Raw[Next + 1]))
			{
				++Next;
			}
			OutPath.push_back(Raw[Next]);
			Anchor = Next;
		}
		return true;
	}
}
