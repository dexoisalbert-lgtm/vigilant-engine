#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaleotrenchSpareTank.generated.h"

class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

// A full spare tank left in the water, with a blinking dive strobe so it can
// be found in the dark. Swim into it to clip it on (if you have room).
// Collected tanks are hidden, not destroyed, so loading an older checkpoint
// can bring them back.
UCLASS()
class PALEOTRENCH_API APaleotrenchSpareTank : public AActor
{
	GENERATED_BODY()

public:
	APaleotrenchSpareTank();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	FName GetPickupId() const;
	bool IsCollected() const { return bCollected; }
	void SetCollected(bool bNewCollected);

protected:
	virtual void BeginPlay() override;

	// Stable id written to save files. Empty uses the actor's name.
	UPROPERTY(EditAnywhere, Category = "Pickup")
	FName PickupId;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> Reach;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Cylinder;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Valve;
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UPointLightComponent> Strobe;

private:
	bool bCollected = false;
	float Clock = 0.0f;
};
