#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/PaleotrenchCaveSettings.h"
#include "PaleotrenchCave.generated.h"

class APaleotrenchAirPocket;
class UDataTable;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UPointLightComponent;
class UPostProcessComponent;
class UProceduralMeshComponent;
struct FPaleotrenchCaveBuild;

// A generated cave (M3). The game mode calls Build with the save's seed; the layout, the rock and the air
// pockets all follow from it (PaleotrenchCore/CaveGraph, CaveField, CaveMesher), so a save's cave never
// changes. The rock is meshed in chunks on worker threads and streamed in and out around the diver; the
// chunks around the entrance are built before the dive starts. Cave space is this actor's local space.
UCLASS()
class PALEOTRENCH_API APaleotrenchCave : public AActor
{
	GENERATED_BODY()

public:
	APaleotrenchCave();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Builds (or rebuilds) the cave for a save seed. The rock around the entrance exists when this returns.
	void Build(uint64 SaveSeed);
	bool IsBuilt() const;

	// In the entrance's air pocket, head just above the water.
	FTransform GetStartTransform() const;
	// Depth reference: the entrance's water line.
	double GetSurfaceWorldZ() const;
	FVector GetArenaLocation() const;
	uint64 GetCaveSeed() const { return CaveSeed; }
	uint8 GetCaveId() const { return CaveId; }
	// One line of statistics for the console (pt.Cave).
	FString Describe() const;
	// A swimmable route through open water (false until the navigation grid is ready).
	bool FindSwimPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;

protected:
	// Which boss cave this is (1 Livyatan, 2 Tusoteuthis, 3 Stygiomedusa, 4 Arthropleura): picks the tuning row
	// and is mixed into the seed, so each cave of a save is different.
	UPROPERTY(EditAnywhere, Category = "Cave", meta = (ClampMin = "1", ClampMax = "4"))
	uint8 CaveId = 1;
	UPROPERTY(EditAnywhere, Category = "Cave")
	TSoftObjectPtr<UDataTable> TuningTable;
	UPROPERTY(EditAnywhere, Category = "Cave")
	TSoftObjectPtr<UMaterialInterface> RockMaterial;
	// Debug: undoes the triangle reversal for Unreal's left-handed frame (the rock then renders inside out).
	UPROPERTY(EditAnywhere, Category = "Cave")
	bool bFlipWinding = false;
	// Chunk meshes finished per frame at most (keeps hitches down while swimming).
	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (ClampMin = "1"))
	int32 ChunksPerFrame = 4;
	// Chunks being meshed on worker threads at once.
	UPROPERTY(EditAnywhere, Category = "Streaming", meta = (ClampMin = "1"))
	int32 MaxChunksInFlight = 8;

	UPROPERTY(VisibleAnywhere, Category = "Underwater")
	TObjectPtr<UExponentialHeightFogComponent> WaterFog;
	UPROPERTY(VisibleAnywhere, Category = "Underwater")
	TObjectPtr<UPostProcessComponent> WaterGrade;

private:
	void Clear();
	void LoadSettings();
	void SpawnAirPockets();
	void SpawnGlows();
	void UpdateStreaming();
	void RequestChunk(const FIntVector& Key);
	void FinishChunks(int32 Budget);
	FVector StreamFocus() const;

	FPaleotrenchCaveSettings Settings;
	TSharedPtr<FPaleotrenchCaveBuild, ESPMode::ThreadSafe> Current;
	uint64 CaveSeed = 0;
	uint32 Generation = 0;
	double NextStreamUpdate = 0.0;
	// Chunk keys the rock surface can pass through, and the ones being meshed right now.
	TSet<FIntVector> Candidates;
	TSet<FIntVector> InFlight;

	UPROPERTY(Transient)
	TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> Chunks;
	UPROPERTY(Transient)
	TArray<TObjectPtr<APaleotrenchAirPocket>> AirPockets;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Glows;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedRockMaterial;
};
