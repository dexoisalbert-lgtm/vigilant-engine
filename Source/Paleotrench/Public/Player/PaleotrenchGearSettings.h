#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PaleotrenchCore/Oxygen.h"
#include "PaleotrenchCore/Power.h"
#include "PaleotrenchGearSettings.generated.h"

// Defaults here are the fallback when a table is missing; keep them equal to
// the CSV rows. Tools/Editor/prepare.py imports the CSVs as DataTables.

// Data/OxygenTuning.csv, row "Default". Pressures in bar, times in seconds.
USTRUCT(BlueprintType)
struct PALEOTRENCH_API FPaleotrenchOxygenSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float TankCapacityBar = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float RestBarPerMinute = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float SwimBarPerMinute = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float SprintBarPerMinute = 28.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float ExertionBarPerMinute = 22.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float ReserveBar = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float CriticalBar = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float AirRefillBarPerSecond = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float SwapSeconds = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	int32 StartSpareTanks = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	int32 MaxSpareTanks = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float EmptyGraceSeconds = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Oxygen")
	float SlowestSwimScale = 0.5f;

	pt::OxygenTuning ToCore() const
	{
		pt::OxygenTuning T;
		T.TankCapacityBar = TankCapacityBar;
		T.RestBarPerMinute = RestBarPerMinute;
		T.SwimBarPerMinute = SwimBarPerMinute;
		T.SprintBarPerMinute = SprintBarPerMinute;
		T.ExertionBarPerMinute = ExertionBarPerMinute;
		T.ReserveBar = ReserveBar;
		T.CriticalBar = CriticalBar;
		T.AirRefillBarPerSecond = AirRefillBarPerSecond;
		T.SwapSeconds = SwapSeconds;
		T.MaxSpareTanks = MaxSpareTanks;
		T.EmptyGraceSeconds = EmptyGraceSeconds;
		T.SlowestSwimScale = SlowestSwimScale;
		return T;
	}

	bool IsValid() const
	{
		return pt::IsValidOxygenTuning(ToCore()) && StartSpareTanks >= 0 && StartSpareTanks <= MaxSpareTanks;
	}
};

// Data/PowerTuning.csv, row "Default". Charge is in abstract units.
USTRUCT(BlueprintType)
struct PALEOTRENCH_API FPaleotrenchPowerSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float PackCapacity = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float StartPackFraction = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float LampCellCapacity = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float StartLampFraction = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float ChargeRatePerSecond = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float ChargeEfficiency = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float CrankRatePerSecond = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float CellLeakPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float LowCellFraction = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Power")
	float BurstSeconds = 1.5f;

	bool IsValid() const
	{
		return pt::IsValidPowerTuning(ToCore({0, 0, 0, 0, 0}))
			&& FMath::IsFinite(StartPackFraction) && StartPackFraction >= 0 && StartPackFraction <= 1
			&& FMath::IsFinite(StartLampFraction) && StartLampFraction >= 0 && StartLampFraction <= 1;
	}

	// Lamp drain rates live in the LampLevels table, one per level.
	pt::PowerTuning ToCore(const float (&LampDrains)[pt::LampLevelCount]) const
	{
		pt::PowerTuning T;
		T.PackCapacity = PackCapacity;
		T.LampCellCapacity = LampCellCapacity;
		T.ChargeRatePerSecond = ChargeRatePerSecond;
		T.ChargeEfficiency = ChargeEfficiency;
		T.CrankRatePerSecond = CrankRatePerSecond;
		T.CellLeakPerSecond = CellLeakPerSecond;
		T.LowCellFraction = LowCellFraction;
		T.BurstSeconds = BurstSeconds;
		for (int32 Index = 0; Index < pt::LampLevelCount; ++Index)
		{
			T.LampDrainPerSecond[Index] = LampDrains[Index];
		}
		return T;
	}
};

// Data/LampLevels.csv: rows Off, Low, Medium, High and Burst.
USTRUCT(BlueprintType)
struct PALEOTRENCH_API FPaleotrenchLampLevelSettings : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	float DrainPerSecond = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	float IntensityLumens = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	float AttenuationRadius = 1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	float InnerConeAngle = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp")
	float OuterConeAngle = 30.0f;

	bool IsValid() const
	{
		return FMath::IsFinite(DrainPerSecond) && DrainPerSecond >= 0 && DrainPerSecond <= 1000
			&& FMath::IsFinite(IntensityLumens) && IntensityLumens >= 0 && IntensityLumens <= 100000
			&& FMath::IsFinite(AttenuationRadius) && AttenuationRadius >= 10 && AttenuationRadius <= 20000
			&& FMath::IsFinite(InnerConeAngle) && InnerConeAngle >= 0 && InnerConeAngle <= 80
			&& FMath::IsFinite(OuterConeAngle) && OuterConeAngle >= InnerConeAngle && OuterConeAngle <= 80;
	}
};
