#include "TestFramework.h"
#include "PaleotrenchCore/Power.h"

#include <cmath>
#include <limits>

namespace
{
	bool Near(double A, double B, double Epsilon = 1e-6)
	{
		return std::abs(A - B) <= Epsilon;
	}

	pt::PowerEvents Run(pt::PowerState& State, pt::EPowerTask Task, double Seconds, int Hz,
		const pt::PowerTuning& Tuning = {})
	{
		pt::PowerEvents All;
		const int Steps = static_cast<int>(Seconds * Hz + 0.5);
		for (int Step = 0; Step < Steps; ++Step)
		{
			const auto Events = pt::StepPower(State, Task, 1.0 / Hz, Tuning);
			All.PackSpent += Events.PackSpent;
			All.Cranked += Events.Cranked;
			All.bLampDied = All.bLampDied || Events.bLampDied;
			All.bBurstEnded = All.bBurstEnded || Events.bBurstEnded;
			All.bCellFull = All.bCellFull || Events.bCellFull;
		}
		return All;
	}
}

PT_TEST(Power_DefaultTuningIsValid)
{
	PT_EXPECT(pt::IsValidPowerTuning({}));
	const auto State = pt::NewPowerState({}, 0.6, 1.0);
	PT_EXPECT(Near(State.Pack, 60.0) && Near(State.LampCell, 20.0));
	PT_EXPECT(State.Level == pt::ELampLevel::Medium);
}

PT_TEST(Power_EachLevelDrainsAtItsRate)
{
	const pt::PowerTuning Tuning;
	const pt::ELampLevel Levels[4] = {
		pt::ELampLevel::Off, pt::ELampLevel::Low, pt::ELampLevel::Medium, pt::ELampLevel::High};
	for (const auto Level : Levels)
	{
		auto State = pt::NewPowerState(Tuning, 1.0, 1.0);
		State.Level = Level;
		Run(State, pt::EPowerTask::None, 10.0, 60, Tuning);
		const double Expected = 20.0 - Tuning.LampDrainPerSecond[static_cast<int>(Level)] * 10.0;
		PT_EXPECT(Near(State.LampCell, Expected));
		PT_EXPECT(State.Pack == 100.0);
	}
}

PT_TEST(Power_ToggleRemembersTheLastLevel)
{
	auto State = pt::NewPowerState({}, 1.0, 1.0);
	State.Level = pt::ELampLevel::High;
	pt::ToggleLamp(State);
	PT_EXPECT(State.Level == pt::ELampLevel::Off);
	pt::ToggleLamp(State);
	PT_EXPECT(State.Level == pt::ELampLevel::High);
}

PT_TEST(Power_StepLevelCoversOffToHigh)
{
	auto State = pt::NewPowerState({}, 1.0, 1.0);
	State.Level = pt::ELampLevel::Low;
	pt::StepLampLevel(State, -1);
	PT_EXPECT(State.Level == pt::ELampLevel::Off);
	pt::StepLampLevel(State, -1);
	PT_EXPECT(State.Level == pt::ELampLevel::Off);
	pt::ToggleLamp(State);
	PT_EXPECT(State.Level == pt::ELampLevel::Low);
	pt::StepLampLevel(State, +1);
	pt::StepLampLevel(State, +1);
	pt::StepLampLevel(State, +5);
	PT_EXPECT(State.Level == pt::ELampLevel::High);
	pt::StepLampLevel(State, 0);
	PT_EXPECT(State.Level == pt::ELampLevel::High);
}

PT_TEST(Power_BurstStrobesThenReturnsToTheSteadyLevel)
{
	const pt::PowerTuning Tuning;
	auto State = pt::NewPowerState(Tuning, 1.0, 1.0);
	State.Level = pt::ELampLevel::Low;
	PT_EXPECT(pt::StartBurst(State, Tuning));
	PT_EXPECT(!pt::StartBurst(State, Tuning));
	PT_EXPECT(pt::EffectiveLampLevel(State) == pt::ELampLevel::Burst);
	const auto Events = Run(State, pt::EPowerTask::None, 1.5, 60, Tuning);
	PT_EXPECT(Events.bBurstEnded);
	PT_EXPECT(pt::EffectiveLampLevel(State) == pt::ELampLevel::Low);
	PT_EXPECT(Near(State.LampCell, 20.0 - 1.2 * 1.5, 1e-3));

	// A burst works from Off too, and leaves the lamp off afterwards.
	State.Level = pt::ELampLevel::Off;
	PT_EXPECT(pt::StartBurst(State, Tuning));
	Run(State, pt::EPowerTask::None, 2.0, 60, Tuning);
	PT_EXPECT(pt::EffectiveLampLevel(State) == pt::ELampLevel::Off);
}

PT_TEST(Power_ChargingMovesPackIntoTheCellWithLosses)
{
	const pt::PowerTuning Tuning;
	auto State = pt::NewPowerState(Tuning, 1.0, 0.0);
	State.Level = pt::ELampLevel::Off;
	const auto Events = Run(State, pt::EPowerTask::ChargeLamp, 2.0, 60, Tuning);
	PT_EXPECT(Near(State.LampCell, 8.0));
	PT_EXPECT(Near(State.Pack, 90.0));
	PT_EXPECT(Near(Events.PackSpent, 10.0));
	PT_EXPECT(!Events.bCellFull);

	const auto Full = Run(State, pt::EPowerTask::ChargeLamp, 10.0, 60, Tuning);
	PT_EXPECT(Full.bCellFull && State.LampCell == 20.0);
	PT_EXPECT(Near(State.Pack, 75.0));
	const auto Again = Run(State, pt::EPowerTask::ChargeLamp, 1.0, 60, Tuning);
	PT_EXPECT(!Again.bCellFull && Again.PackSpent == 0.0);
}

PT_TEST(Power_ChargingStopsWhenThePackIsFlat)
{
	const pt::PowerTuning Tuning;
	auto State = pt::NewPowerState(Tuning, 0.05, 0.0);
	State.Level = pt::ELampLevel::Off;
	Run(State, pt::EPowerTask::ChargeLamp, 10.0, 60, Tuning);
	PT_EXPECT(State.Pack == 0.0);
	PT_EXPECT(Near(State.LampCell, 4.0));
}

PT_TEST(Power_CrankingRefillsThePackSlowly)
{
	const pt::PowerTuning Tuning;
	auto State = pt::NewPowerState(Tuning, 0.0, 1.0);
	const auto Events = Run(State, pt::EPowerTask::Crank, 10.0, 60, Tuning);
	PT_EXPECT(Near(State.Pack, 5.0) && Near(Events.Cranked, 5.0));
	State.Pack = 99.9;
	Run(State, pt::EPowerTask::Crank, 10.0, 60, Tuning);
	PT_EXPECT(State.Pack == 100.0);
}

PT_TEST(Power_FlatCellTurnsTheLampOffOnce)
{
	const pt::PowerTuning Tuning;
	auto State = pt::NewPowerState(Tuning, 1.0, 0.0);
	State.LampCell = 0.05;
	State.Level = pt::ELampLevel::High;
	PT_EXPECT(pt::IsLampLow(State, Tuning));
	const auto Events = Run(State, pt::EPowerTask::None, 1.0, 60, Tuning);
	PT_EXPECT(Events.bLampDied);
	PT_EXPECT(pt::EffectiveLampLevel(State) == pt::ELampLevel::Off);
	PT_EXPECT(State.Level == pt::ELampLevel::High);
	PT_EXPECT(!pt::StartBurst(State, Tuning));
	const auto Later = Run(State, pt::EPowerTask::None, 1.0, 60, Tuning);
	PT_EXPECT(!Later.bLampDied);

	// Charging brings the chosen level straight back.
	Run(State, pt::EPowerTask::ChargeLamp, 0.5, 60, Tuning);
	PT_EXPECT(pt::EffectiveLampLevel(State) == pt::ELampLevel::High);
}

PT_TEST(Power_LeakDrainsTheCell)
{
	pt::PowerTuning Tuning;
	Tuning.CellLeakPerSecond = 0.1;
	auto State = pt::NewPowerState(Tuning, 1.0, 1.0);
	State.Level = pt::ELampLevel::Off;
	Run(State, pt::EPowerTask::None, 10.0, 60, Tuning);
	PT_EXPECT(Near(State.LampCell, 19.0));
}

PT_TEST(Power_FrameRateIndependent)
{
	double Cells[3] = {};
	double Packs[3] = {};
	const int Rates[3] = {30, 60, 144};
	for (int Index = 0; Index < 3; ++Index)
	{
		auto State = pt::NewPowerState({}, 1.0, 0.5);
		State.Level = pt::ELampLevel::Medium;
		Run(State, pt::EPowerTask::ChargeLamp, 3.0, Rates[Index]);
		Cells[Index] = State.LampCell;
		Packs[Index] = State.Pack;
	}
	PT_EXPECT(Near(Cells[0], Cells[1], 1e-6) && Near(Cells[1], Cells[2], 1e-6));
	PT_EXPECT(Near(Packs[0], Packs[1], 1e-6) && Near(Packs[1], Packs[2], 1e-6));
}

PT_TEST(Power_InvalidInputChangesNothing)
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	auto State = pt::NewPowerState({}, 1.0, 1.0);
	pt::StepPower(State, pt::EPowerTask::ChargeLamp, NaN, {});
	pt::PowerTuning Broken;
	Broken.ChargeEfficiency = 0.0;
	PT_EXPECT(!pt::IsValidPowerTuning(Broken));
	pt::StepPower(State, pt::EPowerTask::Crank, 1.0, Broken);
	PT_EXPECT(State.Pack == 100.0 && State.LampCell == 20.0);
}

PT_TEST(Power_SanitiseRepairsLoadedValues)
{
	pt::PowerState State;
	State.Pack = NAN;
	State.LampCell = 500.0;
	State.Level = static_cast<pt::ELampLevel>(9);
	State.LastOnLevel = pt::ELampLevel::Burst;
	State.BurstRemaining = 1.0;
	pt::SanitizePowerState(State, {});
	PT_EXPECT(State.Pack == 0.0 && State.LampCell == 20.0);
	PT_EXPECT(State.Level == pt::ELampLevel::High);
	PT_EXPECT(State.LastOnLevel == pt::ELampLevel::Medium);
	PT_EXPECT(State.BurstRemaining == 0.0);
}
