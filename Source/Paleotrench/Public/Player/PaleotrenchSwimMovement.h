#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PaleotrenchCore/Swim.h"
#include "PaleotrenchSwimMovement.generated.h"

// Single-player diving mode. Translation is engine-free; Unreal owns sweeps,
// penetration recovery and wall sliding. Air-pocket walking arrives in M10.
UCLASS()
class PALEOTRENCH_API UPaleotrenchSwimMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPaleotrenchSwimMovement();
	static constexpr uint8 DivingMode = 0;
	void SetSwimTuning(const pt::SwimTuning& InTuning) { Tuning = InTuning; }
	void SetSprinting(bool bEnabled) { bSprinting = bEnabled; }
	// Multiplies cruise and sprint speed (low oxygen, busy hands). 0.05-1.
	void SetSpeedScale(float Scale) { SpeedScale = FMath::Clamp(Scale, 0.05f, 1.0f); }
	virtual float GetMaxSpeed() const override;

protected:
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

private:
	pt::SwimTuning Tuning;
	bool bSprinting = false;
	float SpeedScale = 1.0f;
};
