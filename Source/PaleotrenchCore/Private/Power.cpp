#include "PaleotrenchCore/Power.h"

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

		bool IsSteadyOnLevel(ELampLevel Level)
		{
			return Level == ELampLevel::Low || Level == ELampLevel::Medium || Level == ELampLevel::High;
		}

		int LevelIndex(ELampLevel Level)
		{
			return static_cast<int>(Level);
		}
	}

	bool IsValidPowerTuning(const PowerTuning& T)
	{
		bool bDrainsValid = true;
		for (const double Drain : T.LampDrainPerSecond)
		{
			bDrainsValid = bDrainsValid && InRange(Drain, 0.0, 1000.0);
		}
		return bDrainsValid
			&& InRange(T.PackCapacity, 0.1, 100000.0)
			&& InRange(T.LampCellCapacity, 0.1, 100000.0)
			&& InRange(T.ChargeRatePerSecond, 0.0, 100000.0)
			&& InRange(T.ChargeEfficiency, 0.01, 1.0)
			&& InRange(T.CrankRatePerSecond, 0.0, 100000.0)
			&& InRange(T.CellLeakPerSecond, 0.0, 1000.0)
			&& InRange(T.LowCellFraction, 0.0, 1.0)
			&& InRange(T.BurstSeconds, 0.1, 10.0);
	}

	PowerState NewPowerState(const PowerTuning& Tuning, double PackFraction, double CellFraction)
	{
		const auto Fraction = [](double Value) { return std::isfinite(Value) ? std::clamp(Value, 0.0, 1.0) : 1.0; };
		PowerState State;
		State.Pack = Tuning.PackCapacity * Fraction(PackFraction);
		State.LampCell = Tuning.LampCellCapacity * Fraction(CellFraction);
		return State;
	}

	void SanitizePowerState(PowerState& State, const PowerTuning& Tuning)
	{
		State.Pack = std::isfinite(State.Pack) ? std::clamp(State.Pack, 0.0, Tuning.PackCapacity) : 0.0;
		State.LampCell = std::isfinite(State.LampCell) ? std::clamp(State.LampCell, 0.0, Tuning.LampCellCapacity) : 0.0;
		if (State.Level != ELampLevel::Off && !IsSteadyOnLevel(State.Level))
		{
			State.Level = ELampLevel::High;
		}
		if (!IsSteadyOnLevel(State.LastOnLevel))
		{
			State.LastOnLevel = ELampLevel::Medium;
		}
		State.BurstRemaining = 0.0;
	}

	ELampLevel EffectiveLampLevel(const PowerState& State)
	{
		if (State.LampCell <= 0.0)
		{
			return ELampLevel::Off;
		}
		if (State.BurstRemaining > 0.0)
		{
			return ELampLevel::Burst;
		}
		return State.Level;
	}

	void ToggleLamp(PowerState& State)
	{
		if (State.Level == ELampLevel::Off)
		{
			State.Level = IsSteadyOnLevel(State.LastOnLevel) ? State.LastOnLevel : ELampLevel::Medium;
		}
		else
		{
			State.LastOnLevel = State.Level;
			State.Level = ELampLevel::Off;
		}
	}

	void StepLampLevel(PowerState& State, int Direction)
	{
		if (Direction == 0)
		{
			return;
		}
		if (IsSteadyOnLevel(State.Level))
		{
			State.LastOnLevel = State.Level;  // dimming to Off remembers where it was
		}
		const int Current = State.Level == ELampLevel::Off ? 0 : LevelIndex(State.Level);
		const int Next = std::clamp(Current + (Direction > 0 ? 1 : -1), 0, LevelIndex(ELampLevel::High));
		State.Level = static_cast<ELampLevel>(Next);
		if (IsSteadyOnLevel(State.Level))
		{
			State.LastOnLevel = State.Level;
		}
	}

	bool StartBurst(PowerState& State, const PowerTuning& Tuning)
	{
		if (State.LampCell <= 0.0 || State.BurstRemaining > 0.0 || !IsValidPowerTuning(Tuning))
		{
			return false;
		}
		State.BurstRemaining = Tuning.BurstSeconds;
		return true;
	}

	bool IsLampLow(const PowerState& State, const PowerTuning& Tuning)
	{
		return State.LampCell < Tuning.LampCellCapacity * Tuning.LowCellFraction;
	}

	PowerEvents StepPower(PowerState& State, EPowerTask Task, double DeltaSeconds, const PowerTuning& Tuning)
	{
		PowerEvents Events;
		if (!std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0 || !IsValidPowerTuning(Tuning))
		{
			return Events;
		}
		const double Dt = std::min(DeltaSeconds, 0.25);
		const double CellBefore = State.LampCell;

		// The lamp burns at whatever level is actually lit during this step.
		const ELampLevel Lit = EffectiveLampLevel(State);
		double Cell = State.LampCell
			- (Tuning.LampDrainPerSecond[LevelIndex(Lit)] + Tuning.CellLeakPerSecond) * Dt;
		if (State.BurstRemaining > 0.0)
		{
			State.BurstRemaining -= Dt;
			// Tolerance so N steps of 1/N s end a 1 s burst on the last step.
			if (State.BurstRemaining <= 1e-9)
			{
				State.BurstRemaining = 0.0;
				Events.bBurstEnded = true;
			}
		}
		Cell = std::max(Cell, 0.0);

		if (Task == EPowerTask::ChargeLamp && State.Pack > 0.0 && Cell < Tuning.LampCellCapacity)
		{
			double Gain = std::min(Tuning.ChargeRatePerSecond * Dt, Tuning.LampCellCapacity - Cell);
			double Spent = Gain / Tuning.ChargeEfficiency;
			if (Spent > State.Pack)
			{
				Spent = State.Pack;
				Gain = Spent * Tuning.ChargeEfficiency;
			}
			State.Pack = std::max(State.Pack - Spent, 0.0);
			Cell += Gain;
			Events.PackSpent = Spent;
			// Absorb rounding so a full cell is exactly full.
			if (Cell >= Tuning.LampCellCapacity - 1e-9)
			{
				Cell = Tuning.LampCellCapacity;
				Events.bCellFull = CellBefore < Tuning.LampCellCapacity;
			}
		}
		else if (Task == EPowerTask::Crank)
		{
			const double Gain = std::min(Tuning.CrankRatePerSecond * Dt, Tuning.PackCapacity - State.Pack);
			State.Pack += std::max(Gain, 0.0);
			Events.Cranked = std::max(Gain, 0.0);
		}

		State.LampCell = std::clamp(Cell, 0.0, Tuning.LampCellCapacity);
		if (CellBefore > 0.0 && State.LampCell <= 0.0)
		{
			Events.bLampDied = true;
			if (State.BurstRemaining > 0.0)
			{
				State.BurstRemaining = 0.0;
				Events.bBurstEnded = true;
			}
		}
		return Events;
	}
}
