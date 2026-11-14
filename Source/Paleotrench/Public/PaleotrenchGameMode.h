#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaleotrenchGameMode.generated.h"

class APaleotrenchAirPocket;
class APaleotrenchCave;
class APaleotrenchDiver;

namespace pt
{
	struct SaveData;
}

// Default game mode for every level (Config/DefaultEngine.ini). Spawns the
// C++ diver and runs the dive session: continuing a save, checkpoint
// autosaves at air pockets, blackout and respawn, and collected pickups.
// In a level with a generated cave (M3) the save's seed builds the cave and
// the dive starts in its entrance air pocket.
// Playtest console commands: pt.Save, pt.Load, pt.NewGame, pt.Oxygen,
// pt.Pack, pt.Lamp, pt.Spares, pt.Wrist, pt.Body, pt.Cave, pt.CavePath.
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
	// Deletes the save and starts over. A given seed (pt.Cave <seed>) picks the cave; otherwise a random one.
	void StartNewGame(TOptional<uint64> WithSeed = TOptional<uint64>());
	APaleotrenchDiver* GetDiver() const { return Diver.Get(); }
	APaleotrenchCave* GetCave() const { return Cave.Get(); }
	uint64 GetSaveSeed() const { return Seed; }

private:
	bool WriteSave(const FVector& Location, float Yaw, FName CheckpointId);
	bool ApplySave(const pt::SaveData& Data);
	void BeginFreshDive();
	void SyncPickups();
	void FinishRespawn();

	// Builds the level's cave (if it has one) for the current seed and sets the diver's depth reference.
	void BuildCave();

	TWeakObjectPtr<APaleotrenchDiver> Diver;
	TWeakObjectPtr<APaleotrenchCave> Cave;
	TOptional<uint64> ForcedSeed;
	TSet<FName> CollectedPickups;
	uint64 Seed = 0;
	double PlaySeconds = 0.0;
	double LastCheckpointTime = -1000.0;
	FName LastCheckpointId;
	FTimerHandle RespawnTimer;
	bool bRespawning = false;
};
