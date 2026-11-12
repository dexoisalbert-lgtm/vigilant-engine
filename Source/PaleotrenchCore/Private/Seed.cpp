#include "PaleotrenchCore/Seed.h"

namespace pt
{
	namespace
	{
		uint64_t Avalanche(uint64_t X)
		{
			// splitmix64 finaliser
			X ^= X >> 30u;
			X *= 0xBF58476D1CE4E5B9ULL;
			X ^= X >> 27u;
			X *= 0x94D049BB133111EBULL;
			X ^= X >> 31u;
			return X;
		}
	}

	const char* CaveName(ECave Cave)
	{
		switch (Cave)
		{
		case ECave::Hub:          return "The Confluence";
		case ECave::Livyatan:     return "The Drowned Cathedral";
		case ECave::Tusoteuthis:  return "The Ink Galleries";
		case ECave::Stygiomedusa: return "The Black Veil";
		case ECave::Arthropleura: return "The Breathing Tunnels";
		}
		return "Unknown";
	}

	uint64_t HashText(std::string_view Text)
	{
		uint64_t Hash = 0xCBF29CE484222325ULL;
		for (const char Ch : Text)
		{
			Hash ^= static_cast<uint64_t>(static_cast<unsigned char>(Ch));
			Hash *= 0x100000001B3ULL;
		}
		return Avalanche(Hash);
	}

	uint64_t MixSeed(uint64_t A, uint64_t B)
	{
		return Avalanche(A + 0x9E3779B97F4A7C15ULL + Avalanche(B));
	}

	uint64_t DeriveCaveSeed(uint64_t SaveSeed, ECave Cave)
	{
		return MixSeed(SaveSeed, 0xCA7E0000ULL + static_cast<uint64_t>(Cave));
	}
}
