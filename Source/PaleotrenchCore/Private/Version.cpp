#include "PaleotrenchCore/Version.h"

#include "PaleotrenchCore/Random.h"
#include "PaleotrenchCore/SaveData.h"
#include "PaleotrenchCore/Seed.h"

namespace pt
{
	const char* CoreVersion()
	{
		return "0.2.0";
	}

	bool RunSelfCheck()
	{
		// PCG32 reference stream (seed 42, stream 54).
		Rng Reference(42u, 54u);
		const uint32_t Expected[6] = {
			0xA15C02B7u, 0x7B47F409u, 0xBA1D3330u, 0x83D2F293u, 0xBFA4784Bu, 0xCBED606Eu,
		};
		for (const uint32_t Value : Expected)
		{
			if (Reference.NextU32() != Value)
			{
				return false;
			}
		}

		// Seed derivation must never change, or existing saves get new caves.
		if (HashText("paleotrench") != 0xC5F347C572C815A1ULL)
		{
			return false;
		}
		if (DeriveCaveSeed(1234567890123456789ULL, ECave::Livyatan) != 0x9DFCD8CBFA9025C6ULL)
		{
			return false;
		}

		// Save files must checksum identically everywhere (standard CRC-32 check value).
		const char* Check = "123456789";
		if (Crc32(reinterpret_cast<const uint8_t*>(Check), 9) != 0xCBF43926u)
		{
			return false;
		}

		// Float generation must be bit-exact.
		Rng Floats(7u, 1u);
		if (Floats.Float01() != 8654233.0f * (1.0f / 16777216.0f))
		{
			return false;
		}
		return true;
	}
}
