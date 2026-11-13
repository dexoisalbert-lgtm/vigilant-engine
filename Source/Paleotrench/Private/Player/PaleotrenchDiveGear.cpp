#include "Player/PaleotrenchDiveGear.h"

#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Paleotrench.h"
#include "PaleotrenchCore/SaveData.h"

namespace
{
	const TCHAR* const LampRowNames[pt::LampLevelCount] = {
		TEXT("Off"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Burst")};

	FPaleotrenchLampLevelSettings LampDefault(float Drain, float Lumens, float Radius, float Inner, float Outer)
	{
		FPaleotrenchLampLevelSettings Level;
		Level.DrainPerSecond = Drain;
		Level.IntensityLumens = Lumens;
		Level.AttenuationRadius = Radius;
		Level.InnerConeAngle = Inner;
		Level.OuterConeAngle = Outer;
		return Level;
	}

	template <typename RowType>
	bool LoadSingleRow(const TSoftObjectPtr<UDataTable>& TablePtr, RowType& Out)
	{
		const UDataTable* Table = TablePtr.LoadSynchronous();
		const RowType* Row = Table ? Table->FindRow<RowType>(TEXT("Default"), TEXT("DiveGear")) : nullptr;
		if (Row && Row->IsValid())
		{
			Out = *Row;
			return true;
		}
		return false;
	}
}

UPaleotrenchDiveGear::UPaleotrenchDiveGear()
{
	PrimaryComponentTick.bCanEverTick = false;
	OxygenTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_OxygenTuning.DT_OxygenTuning")));
	PowerTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_PowerTuning.DT_PowerTuning")));
	LampTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_LampLevels.DT_LampLevels")));
	// Fallbacks, equal to Data/LampLevels.csv.
	LampLevels[0] = LampDefault(0.0f, 0.0f, 1000.0f, 15.0f, 30.0f);
	LampLevels[1] = LampDefault(0.02f, 450.0f, 1300.0f, 14.0f, 28.0f);
	LampLevels[2] = LampDefault(0.05f, 1400.0f, 2200.0f, 18.0f, 36.0f);
	LampLevels[3] = LampDefault(0.12f, 4000.0f, 3400.0f, 22.0f, 44.0f);
	LampLevels[4] = LampDefault(1.2f, 9000.0f, 3600.0f, 30.0f, 55.0f);
}

void UPaleotrenchDiveGear::BeginPlay()
{
	Super::BeginPlay();
	LoadTuning();
	ResetForNewGame();
}

void UPaleotrenchDiveGear::LoadTuning()
{
	const bool bOxygen = LoadSingleRow(OxygenTable, OxygenSettings);
	const bool bPower = LoadSingleRow(PowerTable, PowerSettings);

	bool bLamps = false;
	if (const UDataTable* Table = LampTable.LoadSynchronous())
	{
		FPaleotrenchLampLevelSettings Loaded[pt::LampLevelCount];
		bLamps = true;
		for (int32 Index = 0; Index < pt::LampLevelCount; ++Index)
		{
			const auto* Row = Table->FindRow<FPaleotrenchLampLevelSettings>(LampRowNames[Index], TEXT("DiveGear"));
			bLamps = bLamps && Row && Row->IsValid();
			if (Row)
			{
				Loaded[Index] = *Row;
			}
		}
		if (bLamps)
		{
			for (int32 Index = 0; Index < pt::LampLevelCount; ++Index)
			{
				LampLevels[Index] = Loaded[Index];
			}
		}
	}

	float Drains[pt::LampLevelCount];
	for (int32 Index = 0; Index < pt::LampLevelCount; ++Index)
	{
		Drains[Index] = LampLevels[Index].DrainPerSecond;
	}
	OxygenTuning = OxygenSettings.ToCore();
	PowerTuning = PowerSettings.ToCore(Drains);
	if (!pt::IsValidOxygenTuning(OxygenTuning) || !pt::IsValidPowerTuning(PowerTuning))
	{
		// Defaults are valid by construction; this only guards hand edits.
		OxygenSettings = FPaleotrenchOxygenSettings();
		PowerSettings = FPaleotrenchPowerSettings();
		OxygenTuning = OxygenSettings.ToCore();
		PowerTuning = PowerSettings.ToCore(Drains);
	}

	if (!bOxygen || !bPower || !bLamps)
	{
		UE_LOG(LogPaleotrench, Warning,
			TEXT("Dive gear tuning missing/invalid (oxygen %s, power %s, lamp %s); using safe defaults. Run Tools/Prepare.bat."),
			bOxygen ? TEXT("ok") : TEXT("fallback"), bPower ? TEXT("ok") : TEXT("fallback"),
			bLamps ? TEXT("ok") : TEXT("fallback"));
	}
	UE_LOG(LogPaleotrench, Display, TEXT("M2 dive gear ready: tank %.0f bar, %d/%d spares, pack %.0f, lamp cell %.0f, tuning %s"),
		OxygenSettings.TankCapacityBar, OxygenSettings.StartSpareTanks, OxygenSettings.MaxSpareTanks,
		PowerSettings.PackCapacity, PowerSettings.LampCellCapacity,
		(bOxygen && bPower && bLamps) ? TEXT("CSV") : TEXT("fallback"));
}

void UPaleotrenchDiveGear::ResetForNewGame()
{
	Oxygen = pt::NewOxygenState(OxygenTuning, OxygenSettings.StartSpareTanks);
	Power = pt::NewPowerState(PowerTuning, PowerSettings.StartPackFraction, PowerSettings.StartLampFraction);
	Task = EPaleotrenchHandsTask::None;
	RecentBarPerMinute = OxygenSettings.SwimBarPerMinute;
	StimulusClock = 0.0f;
}

FPaleotrenchGearEvents UPaleotrenchDiveGear::Step(float DeltaSeconds, pt::EBreathing Activity, bool bInAir)
{
	FPaleotrenchGearEvents Events;
	bBreathingAir = bInAir;
	if (Oxygen.bBlackedOut || DeltaSeconds <= 0.0f)
	{
		return Events;
	}

	pt::OxygenInput Breathing;
	Breathing.Activity = Activity;
	Breathing.bBreathingAir = bInAir;
	Breathing.bSwapHeld = Task == EPaleotrenchHandsTask::SwapTank;
	const pt::OxygenEvents O = pt::StepOxygen(Oxygen, Breathing, DeltaSeconds, OxygenTuning);

	const pt::EPowerTask PowerTask = Task == EPaleotrenchHandsTask::ChargeLamp ? pt::EPowerTask::ChargeLamp
		: Task == EPaleotrenchHandsTask::Crank ? pt::EPowerTask::Crank
		: pt::EPowerTask::None;
	const double PackBefore = Power.Pack;
	const pt::PowerEvents P = pt::StepPower(Power, PowerTask, DeltaSeconds, PowerTuning);

	Events.bEnteredReserve = O.bEnteredReserve;
	Events.bEnteredCritical = O.bEnteredCritical;
	Events.bRanDry = O.bRanDry;
	Events.bSwapped = O.bSwapped;
	Events.bBlackout = O.bBlackout;
	Events.bLampDied = P.bLampDied;
	Events.bCellFull = P.bCellFull;
	Events.bPackFlat = PowerTask == pt::EPowerTask::ChargeLamp && PackBefore > 0.0 && Power.Pack <= 0.0;

	// A finished swap frees the hands; holding the key again swaps the next tank.
	if (O.bSwapped || O.bBlackout)
	{
		Task = EPaleotrenchHandsTask::None;
	}

	// Smoothed breathing rate for the dive computer's "air time left".
	const float Target = bInAir ? 0.0f : static_cast<float>(pt::BreathingBarPerMinute(Activity, OxygenTuning));
	RecentBarPerMinute += (Target - RecentBarPerMinute) * (1.0f - FMath::Exp(-DeltaSeconds / 6.0f));

	StimulusClock += DeltaSeconds;
	if (StimulusClock >= 0.5f)
	{
		StimulusClock = 0.0f;
		if (Task == EPaleotrenchHandsTask::ChargeLamp && P.PackSpent > 0.0)
		{
			Emit(EPaleotrenchStimulus::Electric, 1.0f);
		}
		else if (Task == EPaleotrenchHandsTask::Crank)
		{
			Emit(EPaleotrenchStimulus::Sound, 1.2f);
		}
	}
	return Events;
}

bool UPaleotrenchDiveGear::BeginTask(EPaleotrenchHandsTask NewTask)
{
	if (NewTask == EPaleotrenchHandsTask::None || Oxygen.bBlackedOut)
	{
		return false;
	}
	if (Task != EPaleotrenchHandsTask::None)
	{
		return Task == NewTask;
	}
	if ((NewTask == EPaleotrenchHandsTask::SwapTank && Oxygen.SpareTanks <= 0)
		|| (NewTask == EPaleotrenchHandsTask::ChargeLamp && Power.Pack <= 0.0))
	{
		return false;
	}
	Task = NewTask;
	StimulusClock = 0.5f;  // the first spark or crank turn is heard immediately
	return true;
}

void UPaleotrenchDiveGear::EndTask(EPaleotrenchHandsTask EndedTask)
{
	if (Task == EndedTask)
	{
		Task = EPaleotrenchHandsTask::None;
	}
}

void UPaleotrenchDiveGear::ToggleLamp()
{
	pt::ToggleLamp(Power);
}

void UPaleotrenchDiveGear::StepLamp(int32 Direction)
{
	pt::StepLampLevel(Power, Direction);
}

bool UPaleotrenchDiveGear::Burst()
{
	if (!pt::StartBurst(Power, PowerTuning))
	{
		return false;
	}
	Emit(EPaleotrenchStimulus::Light, 4.0f);
	return true;
}

bool UPaleotrenchDiveGear::AddSpareTank()
{
	if (Oxygen.SpareTanks >= OxygenTuning.MaxSpareTanks)
	{
		return false;
	}
	++Oxygen.SpareTanks;
	return true;
}

const FPaleotrenchLampLevelSettings& UPaleotrenchDiveGear::GetLampLevel(pt::ELampLevel Level) const
{
	const int32 Index = FMath::Clamp(static_cast<int32>(Level), 0, pt::LampLevelCount - 1);
	return LampLevels[Index];
}

float UPaleotrenchDiveGear::GetSwapFraction() const
{
	return Task == EPaleotrenchHandsTask::SwapTank
		? FMath::Clamp(static_cast<float>(Oxygen.SwapProgress / OxygenTuning.SwapSeconds), 0.0f, 1.0f)
		: 0.0f;
}

float UPaleotrenchDiveGear::GetAirMinutesLeft() const
{
	if (bBreathingAir)
	{
		return -1.0f;
	}
	return static_cast<float>(Oxygen.PressureBar) / FMath::Max(RecentBarPerMinute, 1.0f);
}

void UPaleotrenchDiveGear::WriteTo(pt::SaveData& Data) const
{
	Data.OxygenBar = Oxygen.PressureBar;
	Data.SpareTanks = Oxygen.SpareTanks;
	Data.PackCharge = Power.Pack;
	Data.LampCharge = Power.LampCell;
	Data.LampLevel = static_cast<uint8_t>(Power.Level);
	Data.LampLastOnLevel = static_cast<uint8_t>(Power.LastOnLevel);
}

void UPaleotrenchDiveGear::ReadFrom(const pt::SaveData& Data)
{
	Oxygen = pt::OxygenState();
	Oxygen.PressureBar = Data.OxygenBar;
	Oxygen.SpareTanks = Data.SpareTanks;
	pt::SanitizeOxygenState(Oxygen, OxygenTuning);
	Power = pt::PowerState();
	Power.Pack = Data.PackCharge;
	Power.LampCell = Data.LampCharge;
	Power.Level = static_cast<pt::ELampLevel>(Data.LampLevel);
	Power.LastOnLevel = static_cast<pt::ELampLevel>(Data.LampLastOnLevel);
	pt::SanitizePowerState(Power, PowerTuning);
	Task = EPaleotrenchHandsTask::None;
	RecentBarPerMinute = OxygenSettings.SwimBarPerMinute;
}

void UPaleotrenchDiveGear::DebugSetOxygen(float Bar)
{
	Oxygen.PressureBar = Bar;
	pt::SanitizeOxygenState(Oxygen, OxygenTuning);
}

void UPaleotrenchDiveGear::DebugSetPack(float Units)
{
	Power.Pack = Units;
	pt::SanitizePowerState(Power, PowerTuning);
}

void UPaleotrenchDiveGear::DebugSetLampCell(float Units)
{
	Power.LampCell = Units;
	pt::SanitizePowerState(Power, PowerTuning);
}

void UPaleotrenchDiveGear::DebugSetSpareTanks(int32 Count)
{
	Oxygen.SpareTanks = Count;
	pt::SanitizeOxygenState(Oxygen, OxygenTuning);
}

void UPaleotrenchDiveGear::Emit(EPaleotrenchStimulus Type, float Strength)
{
	if (const AActor* Owner = GetOwner())
	{
		OnStimulus.Broadcast(Type, Owner->GetActorLocation(), Strength);
	}
}
