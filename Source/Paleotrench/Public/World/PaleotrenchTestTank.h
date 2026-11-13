#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaleotrenchTestTank.generated.h"

class UExponentialHeightFogComponent;
class UPostProcessComponent;

// Reproducible M1 greybox. Placed only in M1_TestTank by the editor script;
// normal game modes do not spawn test geometry into future cave maps.
UCLASS()
class PALEOTRENCH_API APaleotrenchTestTank : public AActor
{
	GENERATED_BODY()

public:
	APaleotrenchTestTank();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Underwater")
	TObjectPtr<UExponentialHeightFogComponent> WaterFog;
	UPROPERTY(VisibleAnywhere, Category = "Underwater")
	TObjectPtr<UPostProcessComponent> WaterGrade;
};
