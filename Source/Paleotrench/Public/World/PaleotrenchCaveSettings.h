#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PaleotrenchCore/CaveField.h"
#include "PaleotrenchCore/CaveGraph.h"
#include "PaleotrenchCore/CaveMesher.h"
#include "PaleotrenchCaveSettings.generated.h"

// One cave's generation and streaming settings, imported from Data/CaveTuning.csv by Tools/Editor/prepare.py
// (one row per boss cave). Lengths are cm. The ranges in IsValid() mirror prepare.py.
USTRUCT(BlueprintType)
struct PALEOTRENCH_API FPaleotrenchCaveSettings : public FTableRowBase
{
	GENERATED_BODY()

	// Layout (pt::CaveLayoutTuning)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	int32 MinChambers = 16;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	int32 MaxChambers = 24;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float RegionLength = 48000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float RegionWidth = 30000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float RegionHeight = 9000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float ChamberRadiusMin = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float ChamberRadiusMax = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float ArenaRadius = 2600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float ChamberGap = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float TunnelRadiusMin = 190.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float TunnelRadiusMax = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float SqueezeRadius = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float SqueezeChance = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float ShaftSlope = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float MaxPassageLength = 9000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	int32 CandidateNeighbours = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float LoopFraction = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float MaxAirSwim = 18000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	float SideRoomChance = 0.6f;

	// Rock surface (pt::CaveFieldTuning)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float BlendRadius = 260.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float LumpStrength = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float LumpSize = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float DetailStrength = 35.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float DetailSize = 280.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float ChamberNoiseShare = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rock")
	float PassageNoiseShare = 0.32f;

	// Meshing and streaming
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming")
	float VoxelSize = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming")
	int32 ChunkCells = 32;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming")
	float StreamRadius = 6500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Streaming")
	float KeepRadius = 9000.0f;

	// Faint bioluminescent glows: lumens of each (0 turns them off) and how many per chamber.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	float GlowLumens = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Look")
	int32 GlowsPerChamber = 1;

	pt::CaveLayoutTuning ToLayout() const
	{
		pt::CaveLayoutTuning T;
		T.MinChambers = MinChambers;
		T.MaxChambers = MaxChambers;
		T.RegionLength = RegionLength;
		T.RegionWidth = RegionWidth;
		T.RegionHeight = RegionHeight;
		T.ChamberRadiusMin = ChamberRadiusMin;
		T.ChamberRadiusMax = ChamberRadiusMax;
		T.ArenaRadius = ArenaRadius;
		T.ChamberGap = ChamberGap;
		T.TunnelRadiusMin = TunnelRadiusMin;
		T.TunnelRadiusMax = TunnelRadiusMax;
		T.SqueezeRadius = SqueezeRadius;
		T.SqueezeChance = SqueezeChance;
		T.ShaftSlope = ShaftSlope;
		T.MaxPassageLength = MaxPassageLength;
		T.CandidateNeighbours = CandidateNeighbours;
		T.LoopFraction = LoopFraction;
		T.MaxAirSwim = MaxAirSwim;
		T.SideRoomChance = SideRoomChance;
		return T;
	}

	pt::CaveFieldTuning ToField() const
	{
		pt::CaveFieldTuning T;
		T.BlendRadius = BlendRadius;
		T.LumpStrength = LumpStrength;
		T.LumpSize = LumpSize;
		T.DetailStrength = DetailStrength;
		T.DetailSize = DetailSize;
		T.ChamberNoiseShare = ChamberNoiseShare;
		T.PassageNoiseShare = PassageNoiseShare;
		return T;
	}

	pt::CaveMeshSettings ToMesh() const
	{
		pt::CaveMeshSettings M;
		M.VoxelSize = VoxelSize;
		M.ChunkCells = ChunkCells;
		return M;
	}

	bool IsValid() const
	{
		const auto In = [](double V, double Lo, double Hi) { return FMath::IsFinite(V) && V >= Lo && V <= Hi; };
		return In(MinChambers, 4, 60) && In(MaxChambers, MinChambers, 80) && In(RegionLength, 5000, 200000)
			&& In(RegionWidth, 5000, 200000) && In(RegionHeight, 2000, 60000) && In(ChamberRadiusMin, 200, 5000)
			&& In(ChamberRadiusMax, ChamberRadiusMin, 8000) && In(ArenaRadius, ChamberRadiusMax, 10000) && In(ChamberGap, 0, 20000)
			&& In(TunnelRadiusMin, 100, 2000) && In(TunnelRadiusMax, TunnelRadiusMin, 3000) && In(SqueezeRadius, 70, TunnelRadiusMin)
			&& In(SqueezeChance, 0, 1) && In(ShaftSlope, 0.1, 10) && In(MaxPassageLength, 1000, 50000)
			&& In(CandidateNeighbours, 2, 12) && In(LoopFraction, 0, 2) && In(MaxAirSwim, 2000, 200000) && In(SideRoomChance, 0, 1)
			&& In(BlendRadius, 10, 2000) && In(LumpStrength, 0, 1000) && In(LumpSize, 50, 20000) && In(DetailStrength, 0, 500)
			&& In(DetailSize, 20, 5000) && In(ChamberNoiseShare, 0, 0.5) && In(PassageNoiseShare, 0, 0.6)
			&& In(VoxelSize, 10, 400) && In(ChunkCells, 8, 64) && In(StreamRadius, 1000, 100000) && In(KeepRadius, StreamRadius, 200000)
			&& In(GlowLumens, 0, 100000) && In(GlowsPerChamber, 0, 8);
	}
};
