#include "TestFramework.h"

#include <algorithm>
#include <array>

#include "PaleotrenchCore/Random.h"

PT_TEST(Rng_MatchesPcg32Reference)
{
	pt::Rng Rng(42u, 54u);
	const uint32_t Expected[6] = {0xA15C02B7u, 0x7B47F409u, 0xBA1D3330u, 0x83D2F293u, 0xBFA4784Bu, 0xCBED606Eu};
	for (const uint32_t Value : Expected)
	{
		PT_EXPECT(Rng.NextU32() == Value);
	}
}

PT_TEST(Rng_SameSeedSameStream)
{
	pt::Rng A(99u, 3u);
	pt::Rng B(99u, 3u);
	for (int i = 0; i < 1000; ++i)
	{
		PT_EXPECT(A.NextU32() == B.NextU32());
	}
}

PT_TEST(Rng_DifferentSeedsDiffer)
{
	pt::Rng A(1u);
	pt::Rng B(2u);
	int Same = 0;
	for (int i = 0; i < 1000; ++i)
	{
		Same += (A.NextU32() == B.NextU32()) ? 1 : 0;
	}
	PT_EXPECT(Same < 5);
}

PT_TEST(Rng_RangeIntStaysInBoundsAndCoversAll)
{
	pt::Rng Rng(5u);
	std::array<int, 7> Hits{};
	for (int i = 0; i < 70000; ++i)
	{
		const int32_t V = Rng.RangeInt(-3, 3);
		PT_EXPECT(V >= -3 && V <= 3);
		if (V >= -3 && V <= 3)
		{
			++Hits[static_cast<size_t>(V + 3)];
		}
	}
	for (const int Count : Hits)
	{
		// Expect ~10000 each; allow a generous margin.
		PT_EXPECT(Count > 9300 && Count < 10700);
	}
}

PT_TEST(Rng_RangeIntEdgeCases)
{
	pt::Rng Rng(11u);
	PT_EXPECT(Rng.RangeInt(4, 4) == 4);
	PT_EXPECT(Rng.RangeInt(9, 2) == 9);
	PT_EXPECT(Rng.Below(0) == 0u);
	PT_EXPECT(Rng.Below(1) == 0u);
	for (int i = 0; i < 100; ++i)
	{
		const int32_t V = Rng.RangeInt(INT32_MIN, INT32_MAX);
		(void)V; // must not overflow or hang
	}
}

PT_TEST(Rng_FloatInUnitInterval)
{
	pt::Rng Rng(21u);
	double Sum = 0.0;
	for (int i = 0; i < 100000; ++i)
	{
		const float F = Rng.Float01();
		PT_EXPECT(F >= 0.0f && F < 1.0f);
		Sum += static_cast<double>(F);
	}
	const double Mean = Sum / 100000.0;
	PT_EXPECT(Mean > 0.49 && Mean < 0.51);
}

PT_TEST(Rng_ChanceRespectsProbability)
{
	pt::Rng Rng(31u);
	int Hits = 0;
	for (int i = 0; i < 100000; ++i)
	{
		Hits += Rng.Chance(0.25f) ? 1 : 0;
	}
	PT_EXPECT(Hits > 24000 && Hits < 26000);
	PT_EXPECT(!Rng.Chance(0.0f));
	PT_EXPECT(Rng.Chance(1.0f));
}

PT_TEST(Rng_ShuffleIsPermutation)
{
	pt::Rng Rng(41u);
	std::array<int, 50> Values{};
	for (size_t i = 0; i < Values.size(); ++i)
	{
		Values[i] = static_cast<int>(i);
	}
	Rng.Shuffle(Values.data(), Values.size());
	bool bMoved = false;
	for (size_t i = 0; i < Values.size(); ++i)
	{
		bMoved = bMoved || Values[i] != static_cast<int>(i);
	}
	PT_EXPECT(bMoved);
	std::sort(Values.begin(), Values.end());
	for (size_t i = 0; i < Values.size(); ++i)
	{
		PT_EXPECT(Values[i] == static_cast<int>(i));
	}
}

PT_TEST(Rng_ForkIsIndependentAndDeterministic)
{
	pt::Rng ParentA(77u);
	pt::Rng ParentB(77u);
	pt::Rng ChildA = ParentA.Fork(1u);
	pt::Rng ChildB = ParentB.Fork(1u);
	for (int i = 0; i < 100; ++i)
	{
		PT_EXPECT(ChildA.NextU32() == ChildB.NextU32());
	}

	// Using the child must not disturb the parent.
	PT_EXPECT(ParentA.NextU32() == ParentB.NextU32());

	// Different tags give different streams.
	pt::Rng ParentC(77u);
	pt::Rng Other = ParentC.Fork(2u);
	pt::Rng ParentD(77u);
	pt::Rng Same = ParentD.Fork(1u);
	int Equal = 0;
	for (int i = 0; i < 100; ++i)
	{
		Equal += (Other.NextU32() == Same.NextU32()) ? 1 : 0;
	}
	PT_EXPECT(Equal < 3);
}
