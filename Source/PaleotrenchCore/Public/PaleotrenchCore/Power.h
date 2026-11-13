#pragma once

#include "PaleotrenchCore/Api.h"

namespace pt
{
	// Head-lamp brightness. Values are stored in save files: never reorder or reuse.
	enum class ELampLevel : unsigned char
	{
		Off = 0,
		Low = 1,
		Medium = 2,
		High = 3,
		Burst = 4,  // strobe: a timed overload, never a steady setting
	};

	inline constexpr int LampLevelCount = 5;

	// What the diver's hands are doing with the battery pack. One at a time.
	enum class EPowerTask : unsigned char
	{
		None,
		ChargeLamp,  // lamp cell plugged into the pack
		Crank,       // hand-cranking the pack: slow, noisy fallback
	};

	// Charge is in abstract units; capacities scale with upgrades later.
	struct PowerTuning
	{
		double PackCapacity = 100.0;
		double LampCellCapacity = 20.0;
		double ChargeRatePerSecond = 4.0;   // lamp-cell units gained per second while plugged in
		double ChargeEfficiency = 0.8;      // cell units gained per pack unit spent
		double CrankRatePerSecond = 0.5;    // pack units generated per second of cranking
		double CellLeakPerSecond = 0.0;     // Nightmare: the cell slowly loses charge
		double LowCellFraction = 0.15;      // below this the lamp flickers
		double BurstSeconds = 1.5;
		double LampDrainPerSecond[LampLevelCount] = {0.0, 0.02, 0.05, 0.12, 1.2};
	};

	struct PowerState
	{
		double Pack = 100.0;
		double LampCell = 20.0;
		ELampLevel Level = ELampLevel::Medium;        // steady setting, Off..High
		ELampLevel LastOnLevel = ELampLevel::Medium;  // restored when switched back on
		double BurstRemaining = 0.0;                  // > 0 while strobing
	};

	struct PowerEvents
	{
		double PackSpent = 0.0;   // pack units moved into the lamp cell this step
		double Cranked = 0.0;     // pack units generated this step
		bool bLampDied = false;   // the cell ran flat this step
		bool bBurstEnded = false;
		bool bCellFull = false;   // charging topped up the cell this step
	};

	PT_API bool IsValidPowerTuning(const PowerTuning& Tuning);

	// Pack and cell filled to the given fractions (clamped to 0..1).
	PT_API PowerState NewPowerState(const PowerTuning& Tuning, double PackFraction, double CellFraction);

	// Makes loaded values safe: clamps charges, fixes invalid levels, stops any burst.
	PT_API void SanitizePowerState(PowerState& State, const PowerTuning& Tuning);

	// The level actually lighting the cave: Burst while strobing, Off when the cell is flat.
	PT_API ELampLevel EffectiveLampLevel(const PowerState& State);

	// Off <-> the last level used.
	PT_API void ToggleLamp(PowerState& State);

	// +1 brighter, -1 dimmer, across Off, Low, Medium and High.
	PT_API void StepLampLevel(PowerState& State, int Direction);

	// Starts a strobe burst. False when the cell is flat or a burst is running.
	PT_API bool StartBurst(PowerState& State, const PowerTuning& Tuning);

	PT_API bool IsLampLow(const PowerState& State, const PowerTuning& Tuning);

	// Lamp drain, burst timing, cell leak, then the hands task. A hitch advances
	// at most 0.25 s. Invalid time or tuning changes nothing.
	PT_API PowerEvents StepPower(PowerState& State, EPowerTask Task, double DeltaSeconds,
		const PowerTuning& Tuning);
}
