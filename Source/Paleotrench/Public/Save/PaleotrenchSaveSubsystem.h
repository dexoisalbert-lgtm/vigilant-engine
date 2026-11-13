#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PaleotrenchCore/SaveData.h"
#include "PaleotrenchSaveSubsystem.generated.h"

// Reads and writes the single save slot. The byte format is the engine-free
// core's (tested in Tools/CoreTests); Unreal's save-game system only stores
// the bytes, so later platforms (Steam Cloud, mobile) need no format change.
// In the editor the file is Saved/SaveGames/Paleotrench0.sav.
UCLASS()
class PALEOTRENCH_API UPaleotrenchSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	bool HasSave() const;
	bool Save(const pt::SaveData& Data);
	pt::ESaveLoadResult Load(pt::SaveData& Out) const;
	bool Delete();

	static const TCHAR* SlotName() { return TEXT("Paleotrench0"); }
};
