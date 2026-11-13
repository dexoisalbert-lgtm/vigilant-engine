#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaleotrenchGameMode.generated.h"

// Default game mode for every level. Set as the project default in
// Config/DefaultEngine.ini. Spawns the C++ diver without a Blueprint subclass.
UCLASS()
class PALEOTRENCH_API APaleotrenchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APaleotrenchGameMode();
};
