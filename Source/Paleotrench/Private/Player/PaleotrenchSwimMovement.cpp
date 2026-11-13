#include "Player/PaleotrenchSwimMovement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

UPaleotrenchSwimMovement::UPaleotrenchSwimMovement()
{
	GravityScale = 0.0f;
	DefaultLandMovementMode = MOVE_Custom;
	DefaultWaterMovementMode = MOVE_Custom;
	bOrientRotationToMovement = false;
	bUseControllerDesiredRotation = false;
	MaxSimulationTimeStep = 1.0f / 60.0f;
	MaxSimulationIterations = 16;
}

float UPaleotrenchSwimMovement::GetMaxSpeed() const
{
	if (MovementMode == MOVE_Custom && CustomMovementMode == DivingMode)
	{
		return static_cast<float>((bSprinting ? Tuning.SprintSpeed : Tuning.CruiseSpeed) * SpeedScale);
	}
	return Super::GetMaxSpeed();
}

void UPaleotrenchSwimMovement::PhysCustom(float DeltaTime, int32 Iterations)
{
	if (CustomMovementMode != DivingMode)
	{
		Super::PhysCustom(DeltaTime, Iterations);
		return;
	}
	if (!HasValidData() || !FMath::IsFinite(DeltaTime) || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const FVector Input = (Acceleration / FMath::Max(GetMaxAcceleration(), 1.0f)).GetClampedToMaxSize(1.0);
	pt::SwimTuning Scaled = Tuning;
	Scaled.CruiseSpeed *= SpeedScale;
	Scaled.SprintSpeed *= SpeedScale;
	float RemainingTime = FMath::Min(DeltaTime, 0.25f);
	while (RemainingTime >= MIN_TICK_TIME && Iterations < MaxSimulationIterations && HasValidData())
	{
		++Iterations;
		const float TimeTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= TimeTick;
		const auto Step = pt::IntegrateSwim({Velocity.X, Velocity.Y, Velocity.Z},
			{Input.X, Input.Y, Input.Z}, TimeTick, bSprinting, Scaled);
		Velocity = FVector(Step.Velocity.X, Step.Velocity.Y, Step.Velocity.Z);
		const FVector Delta(Step.Displacement.X, Step.Displacement.Y, Step.Displacement.Z);

		FHitResult Hit(1.0f);
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.bBlockingHit)
		{
			HandleImpact(Hit, TimeTick, Delta);
			// Remove only inward speed. Tangential momentum survives contact;
			// we never reconstruct velocity from the teleport used for depenetration.
			const FVector FirstNormal = Hit.Normal;
			Velocity -= FirstNormal * FMath::Min(0.0, FVector::DotProduct(Velocity, FirstNormal));
			SlideAlongSurface(Delta, 1.0f - Hit.Time, FirstNormal, Hit, true);
			if (Hit.bBlockingHit)
			{
				Velocity -= Hit.Normal * FMath::Min(0.0, FVector::DotProduct(Velocity, Hit.Normal));
			}
			if (Hit.bStartPenetrating)
			{
				Velocity = FVector::ZeroVector;
			}
		}
	}
	UpdateComponentVelocity();
}
