#include "PaleotrenchGameMode.h"
#include "Player/PaleotrenchDiver.h"

APaleotrenchGameMode::APaleotrenchGameMode()
{
	DefaultPawnClass = APaleotrenchDiver::StaticClass();
}
