#pragma once

// Minimal test framework with no dependencies, so the core tests build
// anywhere a C++20 compiler exists.

#include <cstdio>
#include <vector>

namespace pttest
{
	struct TestCase
	{
		const char* Name;
		void (*Fn)();
	};

	inline std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> Cases;
		return Cases;
	}

	inline int& FailureCount()
	{
		static int Count = 0;
		return Count;
	}

	struct Registrar
	{
		Registrar(const char* Name, void (*Fn)()) { Registry().push_back({Name, Fn}); }
	};
}

#define PT_TEST(Name)                                                        \
	static void PtTest_##Name();                                             \
	static const pttest::Registrar PtTestRegistrar_##Name(#Name, &PtTest_##Name); \
	static void PtTest_##Name()

#define PT_EXPECT(Condition)                                                 \
	do                                                                       \
	{                                                                        \
		if (!(Condition))                                                    \
		{                                                                    \
			++pttest::FailureCount();                                        \
			std::printf("    FAILED %s:%d: %s\n", __FILE__, __LINE__, #Condition); \
		}                                                                    \
	} while (false)
