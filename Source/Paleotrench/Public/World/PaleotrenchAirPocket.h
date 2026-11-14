#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaleotrenchAirPocket.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UPostProcessComponent;
class UStaticMeshComponent;

// A trapped bubble of breathable air under a cave ceiling. While the diver's
// head is inside the box, the tank refills; arriving is a checkpoint and an
// autosave. The rock around it belongs to the level (the test tank's dome, or
// generated cave geometry from M3); this actor is the air, the water surface
// at its bottom and a faint light that makes it findable.
UCLASS()
class PALEOTRENCH_API APaleotrenchAirPocket : public AActor
{
	GENERATED_BODY()

public:
	APaleotrenchAirPocket();
	virtual void OnConstruction(const FTransform& Transform) override;

	bool ContainsPoint(const FVector& WorldPoint) const;
	FName GetCheckpointId() const;
	// For pockets spawned at runtime (generated caves): set before the first checkpoint.
	void SetCheckpointId(FName Id) { CheckpointId = Id; }
	// Where a diver reappears after a blackout: head just above the water.
	FTransform GetRespawnTransform() const;

	static APaleotrenchAirPocket* FindContaining(const UWorld* World, const FVector& WorldPoint);
	static APaleotrenchAirPocket* FindById(const UWorld* World, FName Id);

protected:
	// Half-size of the air volume in cm; its bottom face is the water line.
	UPROPERTY(EditAnywhere, Category = "Air Pocket")
	FVector AirExtent = FVector(200.0, 200.0, 125.0);

	// Stable id written to save files. Empty uses the actor's name.
	UPROPERTY(EditAnywhere, Category = "Air Pocket")
	FName CheckpointId;

	UPROPERTY(VisibleAnywhere, Category = "Air Pocket")
	TObjectPtr<UBoxComponent> AirVolume;
	UPROPERTY(VisibleAnywhere, Category = "Air Pocket")
	TObjectPtr<UStaticMeshComponent> WaterSurface;
	UPROPERTY(VisibleAnywhere, Category = "Air Pocket")
	TObjectPtr<UPostProcessComponent> AirLook;
	UPROPERTY(VisibleAnywhere, Category = "Air Pocket")
	TObjectPtr<UPointLightComponent> Glow;
};
