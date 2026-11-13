#include "PaleotrenchGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Paleotrench.h"
#include "PaleotrenchCore/SaveData.h"
#include "PaleotrenchCore/Seed.h"
#include "Player/PaleotrenchDiveGear.h"
#include "Player/PaleotrenchDiver.h"
#include "Save/PaleotrenchSaveSubsystem.h"
#include "TimerManager.h"
#include "World/PaleotrenchAirPocket.h"
#include "World/PaleotrenchSpareTank.h"

#include <string>

namespace
{
	// Bobbing at the surface of the same pocket does not rewrite the save.
	constexpr double CheckpointCooldownSeconds = 20.0;
	constexpr float BlackoutFadeSeconds = 2.5f;
	constexpr float RespawnDelaySeconds = 3.5f;
	constexpr float WakeFadeSeconds = 1.5f;

	UPaleotrenchSaveSubsystem* SaveSystemOf(const UWorld* World)
	{
		const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
		return Instance ? Instance->GetSubsystem<UPaleotrenchSaveSubsystem>() : nullptr;
	}

	uint64 FreshSeed()
	{
		return pt::MixSeed(static_cast<uint64>(FDateTime::UtcNow().GetTicks()), FPlatformTime::Cycles64());
	}

	void FadeView(APaleotrenchDiver* Diver, float From, float To, float Seconds, bool bHoldWhenDone)
	{
		const auto* Controller = Diver ? Cast<APlayerController>(Diver->GetController()) : nullptr;
		if (Controller && Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraFade(From, To, Seconds, FLinearColor::Black, false, bHoldWhenDone);
		}
	}

	FName NameFromUtf8(const std::string& Text)
	{
		return Text.empty() ? NAME_None : FName(UTF8_TO_TCHAR(Text.c_str()));
	}

	std::string Utf8FromName(FName Name)
	{
		return Name.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*Name.ToString()));
	}
}

APaleotrenchGameMode::APaleotrenchGameMode()
{
	DefaultPawnClass = APaleotrenchDiver::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void APaleotrenchGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PlaySeconds += DeltaSeconds;
}

void APaleotrenchGameMode::RegisterDiver(APaleotrenchDiver* InDiver)
{
	Diver = InDiver;
	pt::SaveData Data;
	const UPaleotrenchSaveSubsystem* Saves = SaveSystemOf(GetWorld());
	const pt::ESaveLoadResult Result = Saves ? Saves->Load(Data) : pt::ESaveLoadResult::Empty;
	if (Result == pt::ESaveLoadResult::Ok && ApplySave(Data))
	{
		UE_LOG(LogPaleotrench, Display, TEXT("Continuing the saved dive at '%s' (%.0f s played). Console: pt.NewGame starts over."),
			*LastCheckpointId.ToString(), PlaySeconds);
		FadeView(InDiver, 1.0f, 0.0f, WakeFadeSeconds, false);
		return;
	}
	if (Result == pt::ESaveLoadResult::NewerVersion)
	{
		UE_LOG(LogPaleotrench, Warning, TEXT("The save was written by a newer build; starting a new dive. The next checkpoint overwrites it."));
	}
	BeginFreshDive();
	UE_LOG(LogPaleotrench, Display, TEXT("New dive started (seed %llu)."), Seed);
}

void APaleotrenchGameMode::BeginFreshDive()
{
	Seed = FreshSeed();
	PlaySeconds = 0.0;
	LastCheckpointId = NAME_None;
	LastCheckpointTime = -1000.0;
	CollectedPickups.Reset();
	SyncPickups();
	APaleotrenchDiver* Current = Diver.Get();
	if (!Current)
	{
		return;
	}
	Current->GetGear()->ResetForNewGame();
	if (const AActor* Start = FindPlayerStart(Current->GetController()))
	{
		Current->RespawnAt(Start->GetActorLocation(), static_cast<float>(Start->GetActorRotation().Yaw));
	}
}

void APaleotrenchGameMode::DiverReachedAir(APaleotrenchDiver* InDiver, const APaleotrenchAirPocket* Pocket)
{
	if (bRespawning || !Pocket || InDiver != Diver.Get())
	{
		return;
	}
	const FName Id = Pocket->GetCheckpointId();
	if (Id == LastCheckpointId && PlaySeconds - LastCheckpointTime < CheckpointCooldownSeconds)
	{
		return;
	}
	const FTransform At = Pocket->GetRespawnTransform();
	if (WriteSave(At.GetLocation(), static_cast<float>(At.Rotator().Yaw), Id))
	{
		LastCheckpointId = Id;
		LastCheckpointTime = PlaySeconds;
		InDiver->NotifyCheckpointSaved();
	}
}

void APaleotrenchGameMode::DiverBlackedOut(APaleotrenchDiver* InDiver)
{
	if (bRespawning || InDiver != Diver.Get())
	{
		return;
	}
	bRespawning = true;
	UE_LOG(LogPaleotrench, Display, TEXT("Diver blacked out; returning to the last checkpoint."));
	FadeView(InDiver, 0.0f, 1.0f, BlackoutFadeSeconds, true);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &APaleotrenchGameMode::FinishRespawn, RespawnDelaySeconds, false);
}

void APaleotrenchGameMode::FinishRespawn()
{
	bRespawning = false;
	if (!LoadLatest())
	{
		// Normal difficulty with no checkpoint yet: back to the start of the dive.
		BeginFreshDive();
	}
	FadeView(Diver.Get(), 1.0f, 0.0f, WakeFadeSeconds, false);
}

bool APaleotrenchGameMode::SaveHere()
{
	const APaleotrenchDiver* Current = Diver.Get();
	return Current && WriteSave(Current->GetActorLocation(), Current->GetViewYaw(), NAME_None);
}

bool APaleotrenchGameMode::LoadLatest()
{
	pt::SaveData Data;
	const UPaleotrenchSaveSubsystem* Saves = SaveSystemOf(GetWorld());
	return Saves && Saves->Load(Data) == pt::ESaveLoadResult::Ok && ApplySave(Data);
}

void APaleotrenchGameMode::StartNewGame()
{
	if (UPaleotrenchSaveSubsystem* Saves = SaveSystemOf(GetWorld()))
	{
		Saves->Delete();
	}
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	bRespawning = false;
	BeginFreshDive();
	FadeView(Diver.Get(), 1.0f, 0.0f, WakeFadeSeconds, false);
	UE_LOG(LogPaleotrench, Display, TEXT("Save deleted; new dive started (seed %llu)."), Seed);
}

bool APaleotrenchGameMode::WriteSave(const FVector& Location, float Yaw, FName CheckpointId)
{
	const APaleotrenchDiver* Current = Diver.Get();
	UPaleotrenchSaveSubsystem* Saves = SaveSystemOf(GetWorld());
	if (!Current || !Saves)
	{
		return false;
	}
	pt::SaveData Data;
	Data.Seed = Seed;
	Data.PlaySeconds = PlaySeconds;
	Data.CheckpointId = Utf8FromName(CheckpointId);
	Data.PositionX = Location.X;
	Data.PositionY = Location.Y;
	Data.PositionZ = Location.Z;
	Data.Yaw = Yaw;
	Current->GetGear()->WriteTo(Data);
	TArray<FName> Pickups = CollectedPickups.Array();
	Pickups.Sort(FNameLexicalLess());
	for (const FName& Pickup : Pickups)
	{
		Data.CollectedPickups.push_back(Utf8FromName(Pickup));
	}
	const bool bSaved = Saves->Save(Data);
	if (bSaved)
	{
		UE_LOG(LogPaleotrench, Display, TEXT("Saved the dive at '%s': %.0f bar, %d spare, pack %.0f, lamp %.1f."),
			CheckpointId.IsNone() ? TEXT("here") : *CheckpointId.ToString(), Data.OxygenBar, Data.SpareTanks,
			Data.PackCharge, Data.LampCharge);
	}
	return bSaved;
}

bool APaleotrenchGameMode::ApplySave(const pt::SaveData& Data)
{
	APaleotrenchDiver* Current = Diver.Get();
	if (!Current)
	{
		return false;
	}
	Seed = Data.Seed;
	PlaySeconds = Data.PlaySeconds;
	CollectedPickups.Reset();
	for (const std::string& Pickup : Data.CollectedPickups)
	{
		CollectedPickups.Add(NameFromUtf8(Pickup));
	}
	LastCheckpointId = NameFromUtf8(Data.CheckpointId);
	LastCheckpointTime = PlaySeconds;

	FVector Location(Data.PositionX, Data.PositionY, Data.PositionZ);
	float Yaw = static_cast<float>(Data.Yaw);
	// Prefer the pocket's current position, so moving it in the editor never
	// strands an old save inside the rock.
	if (const APaleotrenchAirPocket* Pocket = APaleotrenchAirPocket::FindById(GetWorld(), LastCheckpointId))
	{
		const FTransform At = Pocket->GetRespawnTransform();
		Location = At.GetLocation();
		Yaw = static_cast<float>(At.Rotator().Yaw);
	}
	Current->GetGear()->ReadFrom(Data);
	Current->RespawnAt(Location, Yaw);
	SyncPickups();
	return true;
}

void APaleotrenchGameMode::SyncPickups()
{
	for (TActorIterator<APaleotrenchSpareTank> It(GetWorld()); It; ++It)
	{
		It->SetCollected(CollectedPickups.Contains(It->GetPickupId()));
	}
}

#if !UE_BUILD_SHIPPING
namespace
{
	APaleotrenchGameMode* ConsoleGameMode(UWorld* World)
	{
		return World ? World->GetAuthGameMode<APaleotrenchGameMode>() : nullptr;
	}

	UPaleotrenchDiveGear* ConsoleGear(UWorld* World)
	{
		const APaleotrenchGameMode* Mode = ConsoleGameMode(World);
		const APaleotrenchDiver* Current = Mode ? Mode->GetDiver() : nullptr;
		return Current ? Current->GetGear() : nullptr;
	}

	float ConsoleNumber(const TArray<FString>& Args, float Fallback)
	{
		return Args.Num() > 0 ? FCString::Atof(*Args[0]) : Fallback;
	}

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchSave(TEXT("pt.Save"),
		TEXT("Playtest: save the dive right here (normally only air pockets save)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (APaleotrenchGameMode* Mode = ConsoleGameMode(World))
			{
				Mode->SaveHere();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchLoad(TEXT("pt.Load"),
		TEXT("Playtest: reload the last save."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (APaleotrenchGameMode* Mode = ConsoleGameMode(World))
			{
				Mode->LoadLatest();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchNewGame(TEXT("pt.NewGame"),
		TEXT("Delete the save and start a new dive at the PlayerStart."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (APaleotrenchGameMode* Mode = ConsoleGameMode(World))
			{
				Mode->StartNewGame();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchOxygen(TEXT("pt.Oxygen"),
		TEXT("Playtest: set tank pressure in bar, e.g. pt.Oxygen 25"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPaleotrenchDiveGear* Gear = ConsoleGear(World))
			{
				Gear->DebugSetOxygen(ConsoleNumber(Args, 200.0f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchPack(TEXT("pt.Pack"),
		TEXT("Playtest: set battery pack charge (0-100), e.g. pt.Pack 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPaleotrenchDiveGear* Gear = ConsoleGear(World))
			{
				Gear->DebugSetPack(ConsoleNumber(Args, 100.0f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchLamp(TEXT("pt.Lamp"),
		TEXT("Playtest: set the lamp cell charge (0-20), e.g. pt.Lamp 2"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPaleotrenchDiveGear* Gear = ConsoleGear(World))
			{
				Gear->DebugSetLampCell(ConsoleNumber(Args, 20.0f));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchWrist(TEXT("pt.Wrist"),
		TEXT("Raise or lower the wrist dive computer (same as Tab)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			const APaleotrenchGameMode* Mode = ConsoleGameMode(World);
			if (APaleotrenchDiver* Current = Mode ? Mode->GetDiver() : nullptr)
			{
				Current->ToggleWrist();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPaleotrenchSpares(TEXT("pt.Spares"),
		TEXT("Playtest: set the number of spare tanks, e.g. pt.Spares 2"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPaleotrenchDiveGear* Gear = ConsoleGear(World))
			{
				Gear->DebugSetSpareTanks(FMath::RoundToInt(ConsoleNumber(Args, 1.0f)));
			}
		}));
}
#endif
