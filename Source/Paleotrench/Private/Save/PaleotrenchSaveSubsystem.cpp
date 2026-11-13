#include "Save/PaleotrenchSaveSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Paleotrench.h"

bool UPaleotrenchSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName(), 0);
}

bool UPaleotrenchSaveSubsystem::Save(const pt::SaveData& Data)
{
	const std::vector<uint8_t> Bytes = pt::SerializeSave(Data);
	TArray<uint8> Buffer;
	Buffer.Append(Bytes.data(), static_cast<int32>(Bytes.size()));
	const bool bSaved = UGameplayStatics::SaveDataToSlot(Buffer, SlotName(), 0);
	if (!bSaved)
	{
		UE_LOG(LogPaleotrench, Error, TEXT("Could not write save slot %s"), SlotName());
	}
	return bSaved;
}

pt::ESaveLoadResult UPaleotrenchSaveSubsystem::Load(pt::SaveData& Out) const
{
	TArray<uint8> Buffer;
	if (!HasSave() || !UGameplayStatics::LoadDataFromSlot(Buffer, SlotName(), 0))
	{
		return pt::ESaveLoadResult::Empty;
	}
	const pt::ESaveLoadResult Result = pt::DeserializeSave(Buffer.GetData(), static_cast<size_t>(Buffer.Num()), Out);
	if (Result != pt::ESaveLoadResult::Ok)
	{
		UE_LOG(LogPaleotrench, Warning, TEXT("Save slot %s not loaded: %s"), SlotName(), ANSI_TO_TCHAR(pt::SaveLoadResultName(Result)));
	}
	return Result;
}

bool UPaleotrenchSaveSubsystem::Delete()
{
	return !HasSave() || UGameplayStatics::DeleteGameInSlot(SlotName(), 0);
}
