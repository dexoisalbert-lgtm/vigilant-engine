#include "TestFramework.h"
#include "PaleotrenchCore/SaveData.h"

#include <cstring>
#include <limits>
#include <string>

namespace
{
	pt::SaveData Sample()
	{
		pt::SaveData Data;
		Data.Seed = 0xC0FFEE1234567890ULL;
		Data.Difficulty = 1;
		Data.PlaySeconds = 4321.25;
		Data.CheckpointId = "TankDome";
		Data.PositionX = -1300.0;
		Data.PositionY = 12.5;
		Data.PositionZ = 1730.0;
		Data.Yaw = -91.0;
		Data.OxygenBar = 143.75;
		Data.SpareTanks = 2;
		Data.PackCharge = 61.5;
		Data.LampCharge = 3.25;
		Data.LampLevel = 3;
		Data.LampLastOnLevel = 3;
		Data.CollectedPickups = {"SpareTank_Gate", "SpareTank_Floor", ""};
		return Data;
	}

	bool Same(const pt::SaveData& A, const pt::SaveData& B)
	{
		return A.Seed == B.Seed && A.Difficulty == B.Difficulty && A.PlaySeconds == B.PlaySeconds
			&& A.CheckpointId == B.CheckpointId && A.PositionX == B.PositionX
			&& A.PositionY == B.PositionY && A.PositionZ == B.PositionZ && A.Yaw == B.Yaw
			&& A.OxygenBar == B.OxygenBar && A.SpareTanks == B.SpareTanks
			&& A.PackCharge == B.PackCharge && A.LampCharge == B.LampCharge
			&& A.LampLevel == B.LampLevel && A.LampLastOnLevel == B.LampLastOnLevel
			&& A.CollectedPickups == B.CollectedPickups;
	}

	// Rewrites the payload checksum, so a test can target the field checks
	// rather than the CRC.
	void Reseal(std::vector<uint8_t>& Bytes)
	{
		const size_t PayloadSize = Bytes.size() - 16;
		const uint32_t Crc = pt::Crc32(Bytes.data() + 12, PayloadSize);
		for (int Index = 0; Index < 4; ++Index)
		{
			Bytes[12 + PayloadSize + static_cast<size_t>(Index)] = static_cast<uint8_t>((Crc >> (8 * Index)) & 0xFFu);
		}
	}
}

PT_TEST(Save_Crc32MatchesTheStandardCheckValue)
{
	const char* Check = "123456789";
	PT_EXPECT(pt::Crc32(reinterpret_cast<const uint8_t*>(Check), 9) == 0xCBF43926u);
	PT_EXPECT(pt::Crc32(nullptr, 0) == 0u);
}

PT_TEST(Save_RoundTripsEveryField)
{
	const auto Original = Sample();
	const auto Bytes = pt::SerializeSave(Original);
	PT_EXPECT(Bytes.size() > 16 && std::memcmp(Bytes.data(), "PTSV", 4) == 0);
	PT_EXPECT(Bytes[4] == static_cast<uint8_t>(pt::SaveFormatVersion) && Bytes[5] == 0 && Bytes[6] == 0 && Bytes[7] == 0);
	pt::SaveData Loaded;
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Loaded) == pt::ESaveLoadResult::Ok);
	PT_EXPECT(Same(Original, Loaded));
	PT_EXPECT(pt::SerializeSave(Loaded) == Bytes);
}

PT_TEST(Save_DefaultDataRoundTrips)
{
	const pt::SaveData Original;
	const auto Bytes = pt::SerializeSave(Original);
	pt::SaveData Loaded = Sample();
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Loaded) == pt::ESaveLoadResult::Ok);
	PT_EXPECT(Same(Original, Loaded));
}

PT_TEST(Save_RejectsEmptyAndForeignFiles)
{
	pt::SaveData Out = Sample();
	PT_EXPECT(pt::DeserializeSave(nullptr, 0, Out) == pt::ESaveLoadResult::Empty);
	const uint8_t Foreign[] = {'G', 'V', 'A', 'S', 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	PT_EXPECT(pt::DeserializeSave(Foreign, sizeof(Foreign), Out) == pt::ESaveLoadResult::NotASave);
	PT_EXPECT(Same(Out, Sample()));
}

PT_TEST(Save_EveryFlippedByteOrTruncationIsCaught)
{
	const auto Bytes = pt::SerializeSave(Sample());
	int Accepted = 0;
	for (size_t Index = 0; Index < Bytes.size(); ++Index)
	{
		auto Damaged = Bytes;
		Damaged[Index] = static_cast<uint8_t>(Damaged[Index] ^ 0x5Au);
		pt::SaveData Out;
		if (pt::DeserializeSave(Damaged.data(), Damaged.size(), Out) == pt::ESaveLoadResult::Ok)
		{
			++Accepted;
		}
	}
	PT_EXPECT(Accepted == 0);
	for (size_t Length = 1; Length < Bytes.size(); ++Length)
	{
		pt::SaveData Out;
		const auto Result = pt::DeserializeSave(Bytes.data(), Length, Out);
		PT_EXPECT(Result == pt::ESaveLoadResult::Corrupt || Result == pt::ESaveLoadResult::NotASave);
	}
}

PT_TEST(Save_NewerVersionIsRefusedNotMisread)
{
	auto Bytes = pt::SerializeSave(Sample());
	Bytes[4] = static_cast<uint8_t>(pt::SaveFormatVersion + 1);
	pt::SaveData Out;
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Out) == pt::ESaveLoadResult::NewerVersion);
	Bytes[4] = 0;
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Out) == pt::ESaveLoadResult::Corrupt);
}

PT_TEST(Save_ImpossibleValuesAreCorruptEvenWithAValidChecksum)
{
	auto Data = Sample();
	Data.OxygenBar = std::numeric_limits<double>::quiet_NaN();
	auto Bytes = pt::SerializeSave(Data);
	pt::SaveData Out;
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Out) == pt::ESaveLoadResult::Corrupt);

	Data = Sample();
	Data.LampLevel = 7;
	Bytes = pt::SerializeSave(Data);
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Out) == pt::ESaveLoadResult::Corrupt);

	// A pickup count far beyond the data must not allocate or overrun.
	Data = Sample();
	Data.CollectedPickups.clear();
	Bytes = pt::SerializeSave(Data);
	const size_t CountOffset = Bytes.size() - 4 - 4;
	Bytes[CountOffset + 3] = 0x7F;
	Reseal(Bytes);
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Out) == pt::ESaveLoadResult::Corrupt);
}

PT_TEST(Save_OverlongStringsAreCappedOnWrite)
{
	auto Data = Sample();
	Data.CheckpointId = std::string(5000, 'x');
	const auto Bytes = pt::SerializeSave(Data);
	pt::SaveData Out;
	PT_EXPECT(pt::DeserializeSave(Bytes.data(), Bytes.size(), Out) == pt::ESaveLoadResult::Ok);
	PT_EXPECT(Out.CheckpointId.size() == 1024);
}
