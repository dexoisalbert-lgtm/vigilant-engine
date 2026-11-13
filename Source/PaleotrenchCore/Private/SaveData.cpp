#include "PaleotrenchCore/SaveData.h"

#include <cmath>
#include <cstring>

namespace pt
{
	namespace
	{
		constexpr uint8_t Magic[4] = {'P', 'T', 'S', 'V'};
		constexpr size_t HeaderSize = 12;  // magic + version + payload size
		constexpr size_t MaxStringBytes = 1024;
		constexpr uint32_t MaxPickups = 65536;

		class Writer
		{
		public:
			std::vector<uint8_t> Bytes;

			void U8(uint8_t Value) { Bytes.push_back(Value); }

			void U32(uint32_t Value)
			{
				for (int Shift = 0; Shift < 32; Shift += 8)
				{
					Bytes.push_back(static_cast<uint8_t>((Value >> Shift) & 0xFFu));
				}
			}

			void U64(uint64_t Value)
			{
				for (int Shift = 0; Shift < 64; Shift += 8)
				{
					Bytes.push_back(static_cast<uint8_t>((Value >> Shift) & 0xFFu));
				}
			}

			void I32(int32_t Value) { U32(static_cast<uint32_t>(Value)); }

			void F64(double Value)
			{
				uint64_t Bits = 0;
				std::memcpy(&Bits, &Value, sizeof(Bits));
				U64(Bits);
			}

			void Str(const std::string& Value)
			{
				const size_t Length = Value.size() < MaxStringBytes ? Value.size() : MaxStringBytes;
				U32(static_cast<uint32_t>(Length));
				Bytes.insert(Bytes.end(), Value.begin(), Value.begin() + static_cast<std::ptrdiff_t>(Length));
			}
		};

		// Bounds-checked reader: any overrun or impossible value sets bFailed.
		class Reader
		{
		public:
			Reader(const uint8_t* InBytes, size_t InSize) : Bytes(InBytes), Size(InSize) {}

			bool bFailed = false;

			bool AtEnd() const { return Offset == Size; }

			uint8_t U8()
			{
				if (!Need(1))
				{
					return 0;
				}
				return Bytes[Offset++];
			}

			uint32_t U32()
			{
				if (!Need(4))
				{
					return 0;
				}
				uint32_t Value = 0;
				for (int Shift = 0; Shift < 32; Shift += 8)
				{
					Value |= static_cast<uint32_t>(Bytes[Offset++]) << Shift;
				}
				return Value;
			}

			uint64_t U64()
			{
				if (!Need(8))
				{
					return 0;
				}
				uint64_t Value = 0;
				for (int Shift = 0; Shift < 64; Shift += 8)
				{
					Value |= static_cast<uint64_t>(Bytes[Offset++]) << Shift;
				}
				return Value;
			}

			int32_t I32() { return static_cast<int32_t>(U32()); }

			double F64()
			{
				const uint64_t Bits = U64();
				double Value = 0.0;
				std::memcpy(&Value, &Bits, sizeof(Value));
				if (!std::isfinite(Value))
				{
					bFailed = true;
					return 0.0;
				}
				return Value;
			}

			std::string Str()
			{
				const uint32_t Length = U32();
				if (Length > MaxStringBytes || !Need(Length))
				{
					bFailed = true;
					return {};
				}
				std::string Value(reinterpret_cast<const char*>(Bytes + Offset), Length);
				Offset += Length;
				return Value;
			}

		private:
			bool Need(size_t Count)
			{
				if (bFailed || Size - Offset < Count)
				{
					bFailed = true;
					return false;
				}
				return true;
			}

			const uint8_t* Bytes;
			size_t Size;
			size_t Offset = 0;
		};

		uint32_t ReadU32At(const uint8_t* Bytes)
		{
			return static_cast<uint32_t>(Bytes[0]) | (static_cast<uint32_t>(Bytes[1]) << 8)
				| (static_cast<uint32_t>(Bytes[2]) << 16) | (static_cast<uint32_t>(Bytes[3]) << 24);
		}

		void WritePayloadV1(Writer& W, const SaveData& D)
		{
			W.U64(D.Seed);
			W.U8(D.Difficulty);
			W.F64(D.PlaySeconds);
			W.Str(D.CheckpointId);
			W.F64(D.PositionX);
			W.F64(D.PositionY);
			W.F64(D.PositionZ);
			W.F64(D.Yaw);
			W.F64(D.OxygenBar);
			W.I32(D.SpareTanks);
			W.F64(D.PackCharge);
			W.F64(D.LampCharge);
			W.U8(D.LampLevel);
			W.U8(D.LampLastOnLevel);
			const size_t Count = D.CollectedPickups.size() < MaxPickups ? D.CollectedPickups.size() : MaxPickups;
			W.U32(static_cast<uint32_t>(Count));
			for (size_t Index = 0; Index < Count; ++Index)
			{
				W.Str(D.CollectedPickups[Index]);
			}
		}

		bool ReadPayloadV1(Reader& R, SaveData& D)
		{
			D.Seed = R.U64();
			D.Difficulty = R.U8();
			D.PlaySeconds = R.F64();
			D.CheckpointId = R.Str();
			D.PositionX = R.F64();
			D.PositionY = R.F64();
			D.PositionZ = R.F64();
			D.Yaw = R.F64();
			D.OxygenBar = R.F64();
			D.SpareTanks = R.I32();
			D.PackCharge = R.F64();
			D.LampCharge = R.F64();
			D.LampLevel = R.U8();
			D.LampLastOnLevel = R.U8();
			const uint32_t Count = R.U32();
			if (Count > MaxPickups)
			{
				return false;
			}
			D.CollectedPickups.clear();
			for (uint32_t Index = 0; Index < Count && !R.bFailed; ++Index)
			{
				D.CollectedPickups.push_back(R.Str());
			}
			return !R.bFailed && R.AtEnd() && D.Difficulty <= 2 && D.LampLevel <= 4 && D.LampLastOnLevel <= 4;
		}
	}

	uint32_t Crc32(const uint8_t* Bytes, size_t Size)
	{
		uint32_t Crc = 0xFFFFFFFFu;
		for (size_t Index = 0; Index < Size; ++Index)
		{
			Crc ^= Bytes[Index];
			for (int Bit = 0; Bit < 8; ++Bit)
			{
				Crc = (Crc >> 1) ^ (0xEDB88320u & (0u - (Crc & 1u)));
			}
		}
		return ~Crc;
	}

	std::vector<uint8_t> SerializeSave(const SaveData& Data)
	{
		Writer Payload;
		WritePayloadV1(Payload, Data);

		Writer File;
		File.Bytes.reserve(HeaderSize + Payload.Bytes.size() + 4);
		File.Bytes.insert(File.Bytes.end(), Magic, Magic + 4);
		File.U32(SaveFormatVersion);
		File.U32(static_cast<uint32_t>(Payload.Bytes.size()));
		File.Bytes.insert(File.Bytes.end(), Payload.Bytes.begin(), Payload.Bytes.end());
		File.U32(Crc32(Payload.Bytes.data(), Payload.Bytes.size()));
		return File.Bytes;
	}

	ESaveLoadResult DeserializeSave(const uint8_t* Bytes, size_t Size, SaveData& Out)
	{
		if (Bytes == nullptr || Size == 0)
		{
			return ESaveLoadResult::Empty;
		}
		if (Size < 4 || std::memcmp(Bytes, Magic, 4) != 0)
		{
			return ESaveLoadResult::NotASave;
		}
		if (Size < HeaderSize + 4)
		{
			return ESaveLoadResult::Corrupt;
		}
		const uint32_t Version = ReadU32At(Bytes + 4);
		const uint32_t PayloadSize = ReadU32At(Bytes + 8);
		if (static_cast<size_t>(PayloadSize) != Size - HeaderSize - 4)
		{
			return ESaveLoadResult::Corrupt;
		}
		const uint8_t* Payload = Bytes + HeaderSize;
		if (Crc32(Payload, PayloadSize) != ReadU32At(Payload + PayloadSize))
		{
			return ESaveLoadResult::Corrupt;
		}
		if (Version > SaveFormatVersion)
		{
			return ESaveLoadResult::NewerVersion;
		}

		SaveData Loaded;
		Reader R(Payload, PayloadSize);
		bool bOk = false;
		switch (Version)
		{
		case 1: bOk = ReadPayloadV1(R, Loaded); break;
		default: bOk = false; break;
		}
		if (!bOk)
		{
			return ESaveLoadResult::Corrupt;
		}
		Out = Loaded;
		return ESaveLoadResult::Ok;
	}

	const char* SaveLoadResultName(ESaveLoadResult Result)
	{
		switch (Result)
		{
		case ESaveLoadResult::Ok: return "Ok";
		case ESaveLoadResult::Empty: return "Empty";
		case ESaveLoadResult::NotASave: return "NotASave";
		case ESaveLoadResult::NewerVersion: return "NewerVersion";
		case ESaveLoadResult::Corrupt: return "Corrupt";
		}
		return "Unknown";
	}
}
