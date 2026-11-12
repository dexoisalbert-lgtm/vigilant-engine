#include "TestFramework.h"

int main()
{
	int FailedTests = 0;
	for (const pttest::TestCase& Case : pttest::Registry())
	{
		const int Before = pttest::FailureCount();
		Case.Fn();
		const bool bPassed = pttest::FailureCount() == Before;
		std::printf("[%s] %s\n", bPassed ? " OK " : "FAIL", Case.Name);
		if (!bPassed)
		{
			++FailedTests;
		}
	}
	std::printf("\n%zu tests, %d failed\n", pttest::Registry().size(), FailedTests);
	return FailedTests == 0 ? 0 : 1;
}
