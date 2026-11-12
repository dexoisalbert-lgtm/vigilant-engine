#include "TestFramework.h"

#include <cstring>
#include <set>

#include "PaleotrenchCore/Seed.h"
#include "PaleotrenchCore/Version.h"

PT_TEST(Core_SelfCheckPasses)
{
	PT_EXPECT(pt::RunSelfCheck());
	PT_EXPECT(std::strlen(pt::CoreVersion()) > 0);
}

PT_TEST(Seed_HashTextIsStableAndSensitive)
{
	PT_EXPECT(pt::HashText("abyss") == pt::HashText("abyss"));
	PT_EXPECT(pt::HashText("abyss") != pt::HashText("abysS"));
	PT_EXPECT(pt::HashText("") != pt::HashText(" "));
}

PT_TEST(Seed_EachCaveGetsItsOwnSeed)
{
	const uint64_t SaveSeed = pt::HashText("my save");
	std::set<uint64_t> Seeds;
	for (int i = 0; i < pt::CaveCount; ++i)
	{
		Seeds.insert(pt::DeriveCaveSeed(SaveSeed, static_cast<pt::ECave>(i)));
	}
	PT_EXPECT(Seeds.size() == static_cast<size_t>(pt::CaveCount));
	PT_EXPECT(pt::DeriveCaveSeed(SaveSeed, pt::ECave::Livyatan) == pt::DeriveCaveSeed(SaveSeed, pt::ECave::Livyatan));
}

PT_TEST(Seed_NeighbouringSaveSeedsDoNotCollide)
{
	std::set<uint64_t> Seeds;
	for (uint64_t Save = 0; Save < 2000; ++Save)
	{
		for (int i = 0; i < pt::CaveCount; ++i)
		{
			Seeds.insert(pt::DeriveCaveSeed(Save, static_cast<pt::ECave>(i)));
		}
	}
	PT_EXPECT(Seeds.size() == static_cast<size_t>(2000 * pt::CaveCount));
}

PT_TEST(Seed_CaveNamesExist)
{
	for (int i = 0; i < pt::CaveCount; ++i)
	{
		PT_EXPECT(std::strcmp(pt::CaveName(static_cast<pt::ECave>(i)), "Unknown") != 0);
	}
}
