#include "TestFramework.h"
#include "PaleotrenchCore/Oxygen.h"

#include <cmath>
#include <limits>

namespace
{
	bool Near(double A, double B, double Epsilon = 1e-6)
	{
		return std::abs(A - B) <= Epsilon;
	}

	pt::OxygenEvents Run(pt::OxygenState& State, const pt::OxygenInput& Input, double Seconds, int Hz,
		const pt::OxygenTuning& Tuning = {})
	{
		pt::OxygenEvents All;
		const int Steps = static_cast<int>(Seconds * Hz + 0.5);
		for (int Step = 0; Step < Steps; ++Step)
		{
			const auto Events = pt::StepOxygen(State, Input, 1.0 / Hz, Tuning);
			All.bEnteredReserve = All.bEnteredReserve || Events.bEnteredReserve;
			All.bEnteredCritical = All.bEnteredCritical || Events.bEnteredCritical;
			All.bRanDry = All.bRanDry || Events.bRanDry;
			All.bSwapped = All.bSwapped || Events.bSwapped;
			All.bBlackout = All.bBlackout || Events.bBlackout;
		}
		return All;
	}
}

PT_TEST(Oxygen_DefaultTuningIsValid)
{
	PT_EXPECT(pt::IsValidOxygenTuning({}));
	const auto State = pt::NewOxygenState({}, 1);
	PT_EXPECT(State.PressureBar == 200.0);
	PT_EXPECT(State.SpareTanks == 1);
	PT_EXPECT(pt::NewOxygenState({}, 99).SpareTanks == 2);
	PT_EXPECT(pt::NewOxygenState({}, -3).SpareTanks == 0);
}

PT_TEST(Oxygen_RestUsesTheTunedRatePerMinute)
{
	auto State = pt::NewOxygenState({}, 0);
	Run(State, {pt::EBreathing::Rest}, 60.0, 60);
	PT_EXPECT(Near(State.PressureBar, 190.0));
}

PT_TEST(Oxygen_EffortOrdersConsumption)
{
	double Left[4] = {};
	const pt::EBreathing Activities[4] = {
		pt::EBreathing::Rest, pt::EBreathing::Swim, pt::EBreathing::Exertion, pt::EBreathing::Sprint};
	for (int Index = 0; Index < 4; ++Index)
	{
		auto State = pt::NewOxygenState({}, 0);
		Run(State, {Activities[Index]}, 30.0, 60);
		Left[Index] = State.PressureBar;
	}
	PT_EXPECT(Left[0] > Left[1] && Left[1] > Left[2] && Left[2] > Left[3]);
}

PT_TEST(Oxygen_FrameRateIndependent)
{
	double Results[3] = {};
	const int Rates[3] = {30, 60, 144};
	for (int Index = 0; Index < 3; ++Index)
	{
		auto State = pt::NewOxygenState({}, 0);
		Run(State, {pt::EBreathing::Sprint}, 90.0, Rates[Index]);
		Results[Index] = State.PressureBar;
	}
	PT_EXPECT(Near(Results[0], Results[1], 1e-6) && Near(Results[1], Results[2], 1e-6));
}

PT_TEST(Oxygen_AirPocketRefillsWithoutOverfilling)
{
	auto State = pt::NewOxygenState({}, 0);
	State.PressureBar = 12.0;
	pt::OxygenInput Air;
	Air.bBreathingAir = true;
	Air.Activity = pt::EBreathing::Sprint;
	Run(State, Air, 2.0, 60);
	PT_EXPECT(Near(State.PressureBar, 92.0));
	Run(State, Air, 30.0, 60);
	PT_EXPECT(State.PressureBar == 200.0);
}

PT_TEST(Oxygen_WarningsFireOnceWhenCrossed)
{
	auto State = pt::NewOxygenState({}, 0);
	State.PressureBar = 50.05;
	const auto First = Run(State, {pt::EBreathing::Sprint}, 1.0, 60);
	PT_EXPECT(First.bEnteredReserve && !First.bEnteredCritical);
	const auto Second = Run(State, {pt::EBreathing::Sprint}, 1.0, 60);
	PT_EXPECT(!Second.bEnteredReserve);
	State.PressureBar = 20.01;
	const auto Third = Run(State, {pt::EBreathing::Rest}, 1.0, 60);
	PT_EXPECT(Third.bEnteredCritical && !Third.bEnteredReserve);
}

PT_TEST(Oxygen_RunningDryGivesABreathHoldThenBlackout)
{
	auto State = pt::NewOxygenState({}, 0);
	State.PressureBar = 0.1;
	const auto Dry = Run(State, {pt::EBreathing::Sprint}, 1.0, 60);
	PT_EXPECT(Dry.bRanDry && !Dry.bBlackout && State.PressureBar == 0.0);
	const auto Holding = Run(State, {pt::EBreathing::Rest}, 4.0, 60);
	PT_EXPECT(!Holding.bBlackout && !State.bBlackedOut);
	const auto Out = Run(State, {pt::EBreathing::Rest}, 2.0, 60);
	PT_EXPECT(Out.bBlackout && State.bBlackedOut);
	PT_EXPECT(pt::OxygenDistress(State, {}) == 1.0);

	// Blacked out is frozen: even air does nothing until the game revives the diver.
	pt::OxygenInput Air;
	Air.bBreathingAir = true;
	Run(State, Air, 5.0, 60);
	PT_EXPECT(State.PressureBar == 0.0 && State.bBlackedOut);
	pt::SanitizeOxygenState(State, {});
	PT_EXPECT(!State.bBlackedOut && State.EmptySeconds == 0.0);
}

PT_TEST(Oxygen_ReachingAirDuringTheBreathHoldSavesTheDiver)
{
	auto State = pt::NewOxygenState({}, 0);
	State.PressureBar = 0.0;
	Run(State, {pt::EBreathing::Swim}, 5.0, 60);
	PT_EXPECT(!State.bBlackedOut && State.EmptySeconds > 4.9);
	pt::OxygenInput Air;
	Air.bBreathingAir = true;
	Run(State, Air, 0.5, 60);
	PT_EXPECT(State.EmptySeconds == 0.0 && State.PressureBar > 0.0);
}

PT_TEST(Oxygen_SwapNeedsTheFullHoldAndASpare)
{
	auto State = pt::NewOxygenState({}, 1);
	State.PressureBar = 30.0;
	pt::OxygenInput Swap;
	Swap.bSwapHeld = true;
	Run(State, Swap, 3.0, 60);
	PT_EXPECT(State.SpareTanks == 1 && State.SwapProgress > 2.9);

	// Letting go restarts the swap.
	Run(State, {pt::EBreathing::Rest}, 0.1, 60);
	PT_EXPECT(State.SwapProgress == 0.0);

	const auto Done = Run(State, Swap, 4.1, 60);
	PT_EXPECT(Done.bSwapped && State.SpareTanks == 0);
	PT_EXPECT(State.PressureBar > 199.0);

	const auto None = Run(State, Swap, 5.0, 60);
	PT_EXPECT(!None.bSwapped && State.SwapProgress == 0.0);
}

PT_TEST(Oxygen_SwapDuringTheBreathHoldResetsIt)
{
	auto State = pt::NewOxygenState({}, 1);
	State.PressureBar = 0.0;
	pt::OxygenInput Swap;
	Swap.bSwapHeld = true;
	const auto Events = Run(State, Swap, 4.1, 60);
	PT_EXPECT(Events.bSwapped && !Events.bBlackout);
	PT_EXPECT(State.EmptySeconds == 0.0 && !State.bBlackedOut);
}

PT_TEST(Oxygen_DistressAndSlowdownCurves)
{
	const pt::OxygenTuning Tuning;
	pt::OxygenState State = pt::NewOxygenState(Tuning, 0);
	PT_EXPECT(pt::OxygenDistress(State, Tuning) == 0.0);
	PT_EXPECT(pt::OxygenSwimScale(State, Tuning) == 1.0);

	State.PressureBar = 20.0;
	PT_EXPECT(pt::OxygenDistress(State, Tuning) == 0.0);
	State.PressureBar = 10.0;
	PT_EXPECT(Near(pt::OxygenDistress(State, Tuning), 0.3));
	PT_EXPECT(Near(pt::OxygenSwimScale(State, Tuning), 0.75));
	State.PressureBar = 0.0;
	PT_EXPECT(Near(pt::OxygenDistress(State, Tuning), 0.6));
	PT_EXPECT(Near(pt::OxygenSwimScale(State, Tuning), 0.5));
	State.EmptySeconds = 3.0;
	PT_EXPECT(Near(pt::OxygenDistress(State, Tuning), 0.8));
}

PT_TEST(Oxygen_InvalidInputChangesNothing)
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	auto State = pt::NewOxygenState({}, 1);
	pt::StepOxygen(State, {pt::EBreathing::Sprint}, NaN, {});
	pt::StepOxygen(State, {pt::EBreathing::Sprint}, -1.0, {});
	pt::OxygenTuning Broken;
	Broken.CriticalBar = 80.0;  // above reserve
	PT_EXPECT(!pt::IsValidOxygenTuning(Broken));
	pt::StepOxygen(State, {pt::EBreathing::Sprint}, 1.0, Broken);
	PT_EXPECT(State.PressureBar == 200.0);

	// A huge hitch advances at most a quarter second.
	pt::StepOxygen(State, {pt::EBreathing::Rest}, 10.0, {});
	PT_EXPECT(Near(State.PressureBar, 200.0 - 10.0 / 60.0 * 0.25));

	pt::OxygenInput Weird;
	Weird.DrainScale = NaN;
	pt::StepOxygen(State, Weird, 0.1, {});
	PT_EXPECT(std::isfinite(State.PressureBar));
}

PT_TEST(Oxygen_SanitiseRepairsLoadedValues)
{
	pt::OxygenState State;
	State.PressureBar = std::numeric_limits<double>::infinity();
	State.SpareTanks = -4;
	State.bBlackedOut = true;
	State.SwapProgress = 3.0;
	pt::SanitizeOxygenState(State, {});
	PT_EXPECT(State.PressureBar == 200.0 && State.SpareTanks == 0);
	PT_EXPECT(!State.bBlackedOut && State.SwapProgress == 0.0);
	State.PressureBar = 900.0;
	State.SpareTanks = 50;
	pt::SanitizeOxygenState(State, {});
	PT_EXPECT(State.PressureBar == 200.0 && State.SpareTanks == 2);
}
