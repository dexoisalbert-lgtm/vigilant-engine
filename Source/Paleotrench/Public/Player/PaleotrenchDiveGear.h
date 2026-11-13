#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PaleotrenchCore/Oxygen.h"
#include "PaleotrenchCore/Power.h"
#include "PaleotrenchStimulus.h"
#include "Player/PaleotrenchGearSettings.h"
#include "PaleotrenchDiveGear.generated.h"

class UDataTable;

namespace pt
{
	struct SaveData;
}

// What the diver's hands are busy with. One task at a time; each slows swimming.
UENUM(BlueprintType)
enum class EPaleotrenchHandsTask : uint8
{
	None,
	ChargeLamp,  // lamp cell plugged into the battery pack
	Crank,       // hand-cranking the pack: slow, noisy, hard breathing
	SwapTank,    // changing to a spare tank
};

// One-shot results of a gear step, for the diver's feedback.
struct FPaleotrenchGearEvents
{
	bool bEnteredReserve = false;
	bool bEnteredCritical = false;
	bool bRanDry = false;
	bool bSwapped = false;
	bool bBlackout = false;
	bool bLampDied = false;
	bool bCellFull = false;
	bool bPackFlat = false;
};

// Tank, spare tanks, battery pack and lamp cell. The maths is the engine-free
// core (PaleotrenchCore Oxygen/Power); this component owns the state, loads
// the tuning tables and emits stimuli. The diver steps it once per frame.
UCLASS(ClassGroup = (Paleotrench), meta = (BlueprintSpawnableComponent))
class PALEOTRENCH_API UPaleotrenchDiveGear : public UActorComponent
{
	GENERATED_BODY()

public:
	UPaleotrenchDiveGear();

	void ResetForNewGame();
	FPaleotrenchGearEvents Step(float DeltaSeconds, pt::EBreathing Activity, bool bInAir);

	// Returns false (and changes nothing) when the hands are busy with
	// something else or the task has nothing to work with.
	bool BeginTask(EPaleotrenchHandsTask NewTask);
	void EndTask(EPaleotrenchHandsTask EndedTask);
	void CancelTask() { Task = EPaleotrenchHandsTask::None; }
	EPaleotrenchHandsTask GetTask() const { return Task; }
	bool IsBusy() const { return Task != EPaleotrenchHandsTask::None; }

	void ToggleLamp();
	void StepLamp(int32 Direction);
	bool Burst();
	bool AddSpareTank();

	const pt::OxygenState& GetOxygen() const { return Oxygen; }
	const pt::PowerState& GetPower() const { return Power; }
	const pt::OxygenTuning& GetOxygenTuning() const { return OxygenTuning; }
	const pt::PowerTuning& GetPowerTuning() const { return PowerTuning; }
	pt::ELampLevel GetLitLevel() const { return pt::EffectiveLampLevel(Power); }
	const FPaleotrenchLampLevelSettings& GetLampLevel(pt::ELampLevel Level) const;
	bool IsLampLow() const { return pt::IsLampLow(Power, PowerTuning); }
	bool IsBlackedOut() const { return Oxygen.bBlackedOut; }
	bool IsBreathingAir() const { return bBreathingAir; }
	float GetDistress() const { return static_cast<float>(pt::OxygenDistress(Oxygen, OxygenTuning)); }
	float GetSwimScale() const { return static_cast<float>(pt::OxygenSwimScale(Oxygen, OxygenTuning)); }
	float GetSwapFraction() const;
	// Minutes of air left at the recent breathing rate; negative while breathing air.
	float GetAirMinutesLeft() const;

	void WriteTo(pt::SaveData& Data) const;
	void ReadFrom(const pt::SaveData& Data);

	// Console commands for playtesting (pt.Oxygen and friends).
	void DebugSetOxygen(float Bar);
	void DebugSetPack(float Units);
	void DebugSetLampCell(float Units);
	void DebugSetSpareTanks(int32 Count);

	// Sound/Electric/Light events for creature senses (listeners arrive in M4).
	UPROPERTY(BlueprintAssignable, Category = "Diving")
	FPaleotrenchStimulusEvent OnStimulus;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	TSoftObjectPtr<UDataTable> OxygenTable;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	TSoftObjectPtr<UDataTable> PowerTable;
	UPROPERTY(EditDefaultsOnly, Category = "Tuning")
	TSoftObjectPtr<UDataTable> LampTable;

private:
	void LoadTuning();
	void Emit(EPaleotrenchStimulus Type, float Strength);

	FPaleotrenchOxygenSettings OxygenSettings;
	FPaleotrenchPowerSettings PowerSettings;
	FPaleotrenchLampLevelSettings LampLevels[pt::LampLevelCount];
	pt::OxygenTuning OxygenTuning;
	pt::PowerTuning PowerTuning;
	pt::OxygenState Oxygen;
	pt::PowerState Power;
	EPaleotrenchHandsTask Task = EPaleotrenchHandsTask::None;
	float RecentBarPerMinute = 16.0f;
	float StimulusClock = 0.0f;
	bool bBreathingAir = false;
};
