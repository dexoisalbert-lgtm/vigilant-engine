#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaleotrenchGameMode.generated.h"

class APaleotrenchAirPocket;
class APaleotrenchDiver;

namespace pt
{
	struct SaveData;
}

// Default game mode for every level (Config/DefaultEngine.ini). Spawns the
// C++ diver and runs the dive session: continuing a save, checkpoint
// autosaves at air pockets, blackout and respawn, and collected pickups.
// Playtest console commands: pt.Save, pt.Load, pt.NewGame, pt.Oxygen,
// pt.Pack, pt.Lamp, pt.Spares, pt.Wrist, pt.Body.
UCLASS()
class PALEOTRENCH_API APaleotrenchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APaleotrenchGameMode();
	virtual void Tick(float DeltaSeconds) override;

	// The diver calls this once from BeginPlay; continues the saved dive if any.
	void RegisterDiver(APaleotrenchDiver* InDiver);
	void DiverReachedAir(APaleotrenchDiver* InDiver, const APaleotrenchAirPocket* Pocket);
	void DiverBlackedOut(APaleotrenchDiver* InDiver);

	void PickupCollected(FName PickupId) { CollectedPickups.Add(PickupId); }
	bool IsPickupCollected(FName PickupId) const { return CollectedPickups.Contains(PickupId); }

	bool SaveHere();
	bool LoadLatest();
	void StartNewGame();
	APaleotrenchDiver* GetDiver() const { return Diver.Get(); }

private:
	bool WriteSave(const FVector& Location, float Yaw, FName CheckpointId);
	bool ApplySave(const pt::SaveData& Data);
	void BeginFreshDive();
	void SyncPickups();
	void FinishRespawn();

	TWeakObjectPtr<APaleotrenchDiver> Diver;
	TSet<FName> CollectedPickups;
	uint64 Seed = 0;
	double PlaySeconds = 0.0;
	double LastCheckpointTime = -1000.0;
	FName LastCheckpointId;
	FTimerHandle RespawnTimer;
	bool bRespawning = false;
};
