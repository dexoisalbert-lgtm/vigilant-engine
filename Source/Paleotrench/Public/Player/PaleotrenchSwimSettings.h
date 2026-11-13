#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PaleotrenchCore/Swim.h"
#include "PaleotrenchSwimSettings.generated.h"

// Imported from Data/SwimTuning.csv by Tools/Editor/prepare_m1.py.
USTRUCT(BlueprintType)
struct PALEOTRENCH_API FPaleotrenchSwimSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim")
	float CruiseSpeed = 340.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim")
	float SprintSpeed = 600.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim")
	float DragPerSecond = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim")
	float BuoyancyAcceleration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float MouseSensitivity = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float GamepadTurnRate = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float RollRate = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float SprintTurnScale = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	float FieldOfView = 85.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bInvertLookY = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Swim")
	float FinKickInterval = 0.8f;

	pt::SwimTuning ToCore() const
	{
		return {CruiseSpeed, SprintSpeed, DragPerSecond, BuoyancyAcceleration};
	}

	bool IsValid() const
	{
		return pt::IsValidSwimTuning(ToCore())
			&& FMath::IsFinite(MouseSensitivity) && MouseSensitivity > 0 && MouseSensitivity <= 5
			&& FMath::IsFinite(GamepadTurnRate) && GamepadTurnRate > 0 && GamepadTurnRate <= 360
			&& FMath::IsFinite(RollRate) && RollRate >= 0 && RollRate <= 180
			&& FMath::IsFinite(SprintTurnScale) && SprintTurnScale > 0 && SprintTurnScale <= 1
			&& FMath::IsFinite(FieldOfView) && FieldOfView >= 60 && FieldOfView <= 120
			&& FMath::IsFinite(FinKickInterval) && FinKickInterval >= 0.1f && FinKickInterval <= 5;
	}
};
