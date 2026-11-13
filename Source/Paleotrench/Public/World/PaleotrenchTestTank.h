#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaleotrenchTestTank.generated.h"

class UExponentialHeightFogComponent;
class UPostProcessComponent;

// Reproducible greybox test tank (M1, air dome from M2). Placed only in
// M1_TestTank by the editor script;
// normal game modes do not spawn test geometry into future cave maps.
UCLASS()
class PALEOTRENCH_API APaleotrenchTestTank : public AActor
{
	GENERATED_BODY()

public:
	APaleotrenchTestTank();

	// Water line inside the air dome, relative to the tank. Depth is measured
	// from here, like a dive computer measuring from the surface.
	static constexpr double SurfaceZ = 1700.0;
	double GetSurfaceWorldZ() const { return GetActorLocation().Z + SurfaceZ; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Underwater")
	TObjectPtr<UExponentialHeightFogComponent> WaterFog;
	UPROPERTY(VisibleAnywhere, Category = "Underwater")
	TObjectPtr<UPostProcessComponent> WaterGrade;
};
