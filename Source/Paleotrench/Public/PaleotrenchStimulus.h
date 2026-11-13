#pragma once

#include "CoreMinimal.h"
#include "PaleotrenchStimulus.generated.h"

// Typed stimuli that every player action emits and creatures sense
// differently (GDD section 4). M2 gear emits them; M4 adds the listeners.
UENUM(BlueprintType)
enum class EPaleotrenchStimulus : uint8
{
	Sound,      // fin kicks, cranking, drilling, weapons, radio
	Light,      // lamp level, burst strobe, flares
	Vibration,  // touching rock, mining, walking in air pockets
	Electric,   // charging devices, eel discharge
	Blood,      // wounds and corpses
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPaleotrenchStimulusEvent,
	EPaleotrenchStimulus, Type, FVector, Location, float, Strength);
