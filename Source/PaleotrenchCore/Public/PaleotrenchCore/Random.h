#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace pt
{
	// Deterministic random stream (PCG32). The standard library's distributions
	// differ between compilers, so all gameplay randomness goes through this to
	// guarantee the same seed gives the same cave on every platform.
	struct Rng
	{
		uint64_t State = 0;
		uint64_t Inc = 1;

		Rng() { Seed(0, 0); }
		explicit Rng(uint64_t InSeed, uint64_t InStream = 0) { Seed(InSeed, InStream); }

		void Seed(uint64_t InSeed, uint64_t InStream = 0)
		{
			State = 0;
			Inc = (InStream << 1u) | 1u;
			NextU32();
			State += InSeed;
			NextU32();
		}

		uint32_t NextU32()
		{
			const uint64_t Old = State;
			State = Old * 6364136223846793005ULL + Inc;
			const uint32_t XorShifted = static_cast<uint32_t>(((Old >> 18u) ^ Old) >> 27u);
			const uint32_t Rot = static_cast<uint32_t>(Old >> 59u);
			return (XorShifted >> Rot) | (XorShifted << ((0u - Rot) & 31u));
		}

		uint64_t NextU64()
		{
			const uint64_t Hi = NextU32();
			return (Hi << 32u) | NextU32();
		}

		// Uniform in [0, Bound). Bound of 0 returns 0. Unbiased.
		uint32_t Below(uint32_t Bound)
		{
			if (Bound == 0)
			{
				return 0;
			}
			const uint32_t Threshold = (0u - Bound) % Bound;
			for (;;)
			{
				const uint32_t R = NextU32();
				if (R >= Threshold)
				{
					return R % Bound;
				}
			}
		}

		// Uniform integer in [Lo, Hi] inclusive. Returns Lo if Hi <= Lo.
		int32_t RangeInt(int32_t Lo, int32_t Hi)
		{
			if (Hi <= Lo)
			{
				return Lo;
			}
			const uint32_t Span = static_cast<uint32_t>(static_cast<int64_t>(Hi) - static_cast<int64_t>(Lo));
			const uint32_t Offset = (Span == 0xFFFFFFFFu) ? NextU32() : Below(Span + 1u);
			return static_cast<int32_t>(static_cast<int64_t>(Lo) + static_cast<int64_t>(Offset));
		}

		// Uniform in [0, 1). Built from 24 bits so the result is exact on every platform.
		float Float01()
		{
			return static_cast<float>(NextU32() >> 8u) * (1.0f / 16777216.0f);
		}

		float RangeFloat(float Lo, float Hi)
		{
			return Lo + (Hi - Lo) * Float01();
		}

		// True with the given probability (clamped to 0..1).
		bool Chance(float Probability)
		{
			return Float01() < Probability;
		}

		// Fisher-Yates shuffle of Count elements starting at First.
		template <typename T>
		void Shuffle(T* First, size_t Count)
		{
			for (size_t i = Count; i > 1; --i)
			{
				const size_t j = static_cast<size_t>(Below(static_cast<uint32_t>(i)));
				std::swap(First[i - 1], First[j]);
			}
		}

		// Independent child stream for a sub-system, so adding random calls in
		// one system never changes the results of another.
		Rng Fork(uint64_t Tag)
		{
			const uint64_t ChildSeed = NextU64();
			return Rng(ChildSeed ^ (Tag * 0x9E3779B97F4A7C15ULL), Tag);
		}
	};
}
