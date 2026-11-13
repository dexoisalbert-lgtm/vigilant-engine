#include "PaleotrenchCore/Oxygen.h"

#include <algorithm>
#include <cmath>

namespace pt
{
	namespace
	{
		bool InRange(double Value, double Low, double High)
		{
			return std::isfinite(Value) && Value >= Low && Value <= High;
		}
	}

	bool IsValidOxygenTuning(const OxygenTuning& T)
	{
		return InRange(T.TankCapacityBar, 1.0, 1000.0)
			&& InRange(T.RestBarPerMinute, 0.0, 1000.0)
			&& InRange(T.SwimBarPerMinute, 0.0, 1000.0)
			&& InRange(T.SprintBarPerMinute, 0.0, 1000.0)
			&& InRange(T.ExertionBarPerMinute, 0.0, 1000.0)
			&& InRange(T.ReserveBar, 0.0, T.TankCapacityBar)
			&& InRange(T.CriticalBar, 0.01, T.ReserveBar)
			&& InRange(T.AirRefillBarPerSecond, 0.01, 1000.0)
			&& InRange(T.SwapSeconds, 0.1, 60.0)
			&& T.MaxSpareTanks >= 0 && T.MaxSpareTanks <= 10
			&& InRange(T.EmptyGraceSeconds, 0.0, 60.0)
			&& InRange(T.SlowestSwimScale, 0.05, 1.0);
	}

	double BreathingBarPerMinute(EBreathing Activity, const OxygenTuning& T)
	{
		switch (Activity)
		{
		case EBreathing::Swim: return T.SwimBarPerMinute;
		case EBreathing::Sprint: return T.SprintBarPerMinute;
		case EBreathing::Exertion: return T.ExertionBarPerMinute;
		case EBreathing::Rest:
		default: return T.RestBarPerMinute;
		}
	}

	OxygenState NewOxygenState(const OxygenTuning& Tuning, int SpareTanks)
	{
		OxygenState State;
		State.PressureBar = Tuning.TankCapacityBar;
		State.SpareTanks = std::clamp(SpareTanks, 0, std::max(Tuning.MaxSpareTanks, 0));
		return State;
	}

	void SanitizeOxygenState(OxygenState& State, const OxygenTuning& Tuning)
	{
		State.PressureBar = std::isfinite(State.PressureBar)
			? std::clamp(State.PressureBar, 0.0, Tuning.TankCapacityBar)
			: Tuning.TankCapacityBar;
		State.SpareTanks = std::clamp(State.SpareTanks, 0, std::max(Tuning.MaxSpareTanks, 0));
		State.SwapProgress = 0.0;
		State.EmptySeconds = 0.0;
		State.bBlackedOut = false;
	}

	OxygenEvents StepOxygen(OxygenState& State, const OxygenInput& Input,
		double DeltaSeconds, const OxygenTuning& Tuning)
	{
		OxygenEvents Events;
		if (State.bBlackedOut || !std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0
			|| !IsValidOxygenTuning(Tuning))
		{
			return Events;
		}
		const double Dt = std::min(DeltaSeconds, 0.25);
		const double Before = State.PressureBar;

		if (Input.bBreathingAir)
		{
			// Breathing the pocket's air: the tank is spared and topped up.
			State.PressureBar = std::min(Tuning.TankCapacityBar,
				State.PressureBar + Tuning.AirRefillBarPerSecond * Dt);
		}
		else
		{
			const double Scale = std::isfinite(Input.DrainScale) ? std::max(Input.DrainScale, 0.0) : 1.0;
			const double Used = BreathingBarPerMinute(Input.Activity, Tuning) * Scale / 60.0 * Dt;
			State.PressureBar = std::max(0.0, State.PressureBar - Used);
		}

		if (Input.bSwapHeld && State.SpareTanks > 0)
		{
			State.SwapProgress += Dt;
			if (State.SwapProgress >= Tuning.SwapSeconds)
			{
				State.PressureBar = Tuning.TankCapacityBar;
				--State.SpareTanks;
				State.SwapProgress = 0.0;
				Events.bSwapped = true;
			}
		}
		else
		{
			State.SwapProgress = 0.0;
		}

		if (State.PressureBar > 0.0 || Input.bBreathingAir)
		{
			State.EmptySeconds = 0.0;
		}
		else
		{
			Events.bRanDry = Before > 0.0;
			State.EmptySeconds += Dt;
			if (State.EmptySeconds >= Tuning.EmptyGraceSeconds)
			{
				State.bBlackedOut = true;
				Events.bBlackout = true;
			}
		}

		Events.bEnteredReserve = Before > Tuning.ReserveBar && State.PressureBar <= Tuning.ReserveBar;
		Events.bEnteredCritical = Before > Tuning.CriticalBar && State.PressureBar <= Tuning.CriticalBar;
		return Events;
	}

	double OxygenDistress(const OxygenState& State, const OxygenTuning& Tuning)
	{
		if (State.bBlackedOut)
		{
			return 1.0;
		}
		if (!IsValidOxygenTuning(Tuning) || State.PressureBar >= Tuning.CriticalBar)
		{
			return 0.0;
		}
		if (State.PressureBar > 0.0)
		{
			return 0.6 * (1.0 - State.PressureBar / Tuning.CriticalBar);
		}
		const double Hold = Tuning.EmptyGraceSeconds > 0.0
			? std::min(State.EmptySeconds / Tuning.EmptyGraceSeconds, 1.0)
			: 1.0;
		return 0.6 + 0.4 * Hold;
	}

	double OxygenSwimScale(const OxygenState& State, const OxygenTuning& Tuning)
	{
		if (!IsValidOxygenTuning(Tuning) || State.PressureBar >= Tuning.CriticalBar)
		{
			return 1.0;
		}
		const double Empty = 1.0 - std::max(State.PressureBar, 0.0) / Tuning.CriticalBar;
		return 1.0 - (1.0 - Tuning.SlowestSwimScale) * Empty;
	}
}
