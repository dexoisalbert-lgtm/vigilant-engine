#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PaleotrenchGameMode.generated.h"

// Default game mode for every level. Set as the project default in
// Config/DefaultEngine.ini. Gains the diver pawn and controller in M1.
UCLASS()
class PALEOTRENCH_API APaleotrenchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APaleotrenchGameMode();
};
