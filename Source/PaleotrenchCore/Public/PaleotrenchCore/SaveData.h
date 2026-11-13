#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "PaleotrenchCore/Api.h"

namespace pt
{
	// Everything a save file holds. Engine-free so the byte format is tested
	// here and identical on every platform. Units match Unreal: cm, degrees.
	struct SaveData
	{
		uint64_t Seed = 0;              // fixes every cave for the lifetime of the save
		uint8_t Difficulty = 0;         // 0 Normal, 1 Hard, 2 Nightmare (M6)
		double PlaySeconds = 0.0;

		std::string CheckpointId;       // air pocket that wrote the save; empty = saved elsewhere
		double PositionX = 0.0;
		double PositionY = 0.0;
		double PositionZ = 0.0;
		double Yaw = 0.0;

		double OxygenBar = 0.0;
		int32_t SpareTanks = 0;
		double PackCharge = 0.0;
		double LampCharge = 0.0;
		uint8_t LampLevel = 0;          // pt::ELampLevel
		uint8_t LampLastOnLevel = 2;    // pt::ELampLevel

		std::vector<std::string> CollectedPickups;  // ids of pickups already taken
	};

	enum class ESaveLoadResult : uint8_t
	{
		Ok,
		Empty,         // no bytes at all
		NotASave,      // wrong file signature
		NewerVersion,  // written by a newer build; never overwrite it silently
		Corrupt,       // truncated, checksum mismatch, impossible values
	};

	// Bump when the layout changes, and teach DeserializeSave to read the old one.
	inline constexpr uint32_t SaveFormatVersion = 1;

	// Layout: "PTSV", u32 version, u32 payload size, payload, u32 CRC-32 of the
	// payload. All integers little-endian; doubles as IEEE-754 bit patterns.
	PT_API std::vector<uint8_t> SerializeSave(const SaveData& Data);

	// On anything but Ok, Out is left unchanged.
	PT_API ESaveLoadResult DeserializeSave(const uint8_t* Bytes, size_t Size, SaveData& Out);

	PT_API const char* SaveLoadResultName(ESaveLoadResult Result);

	// Standard CRC-32 (IEEE 802.3, as used by zip and PNG).
	PT_API uint32_t Crc32(const uint8_t* Bytes, size_t Size);
}
