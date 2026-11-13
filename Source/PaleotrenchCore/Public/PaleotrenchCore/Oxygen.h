#pragma once

#include "PaleotrenchCore/Api.h"

namespace pt
{
	// What the diver is doing; sets the breathing rate.
	enum class EBreathing : unsigned char
	{
		Rest,      // hovering still
		Swim,      // moving under normal effort
		Sprint,    // kicking hard
		Exertion,  // heavy work with the hands (hand-cranking the battery pack)
	};

	// Pressures are in bar, like a real dive computer; times are in seconds.
	struct OxygenTuning
	{
		double TankCapacityBar = 200.0;
		double RestBarPerMinute = 10.0;
		double SwimBarPerMinute = 16.0;
		double SprintBarPerMinute = 28.0;
		double ExertionBarPerMinute = 22.0;
		double ReserveBar = 50.0;          // first warning
		double CriticalBar = 20.0;         // distress effects and slowdown start here
		double AirRefillBarPerSecond = 40.0;
		double SwapSeconds = 4.0;          // hold time to change to a spare tank
		int MaxSpareTanks = 2;
		double EmptyGraceSeconds = 6.0;    // breath-hold once the tank is dry, then blackout
		double SlowestSwimScale = 0.5;     // movement speed multiplier when out of air
	};

	struct OxygenState
	{
		double PressureBar = 200.0;
		int SpareTanks = 0;
		double SwapProgress = 0.0;  // seconds held toward a tank swap
		double EmptySeconds = 0.0;  // seconds without air since the tank ran dry
		bool bBlackedOut = false;
	};

	struct OxygenInput
	{
		EBreathing Activity = EBreathing::Rest;
		bool bBreathingAir = false;  // the diver's head is inside an air pocket
		bool bSwapHeld = false;
		double DrainScale = 1.0;     // panic, wounds and upgrades scale breathing (later milestones)
	};

	// One-shot notifications from a single step.
	struct OxygenEvents
	{
		bool bEnteredReserve = false;
		bool bEnteredCritical = false;
		bool bRanDry = false;
		bool bSwapped = false;
		bool bBlackout = false;
	};

	PT_API bool IsValidOxygenTuning(const OxygenTuning& Tuning);

	PT_API double BreathingBarPerMinute(EBreathing Activity, const OxygenTuning& Tuning);

	// A full tank and the given number of spares (clamped to the maximum).
	PT_API OxygenState NewOxygenState(const OxygenTuning& Tuning, int SpareTanks);

	// Makes loaded or hand-edited values safe: clamps ranges, replaces
	// non-finite numbers and revives a blacked-out diver.
	PT_API void SanitizeOxygenState(OxygenState& State, const OxygenTuning& Tuning);

	// Advances breathing, air-pocket refills, tank swaps and the breath-hold
	// after running dry. A hitch advances at most 0.25 s. Invalid time or tuning
	// changes nothing. A blacked-out diver stays frozen until sanitised/reset.
	PT_API OxygenEvents StepOxygen(OxygenState& State, const OxygenInput& Input,
		double DeltaSeconds, const OxygenTuning& Tuning);

	// 0 above the critical pressure, rising to 0.6 as the tank empties and to 1
	// at blackout. Drives tunnel vision and the heartbeat.
	PT_API double OxygenDistress(const OxygenState& State, const OxygenTuning& Tuning);

	// Movement speed multiplier: 1 above critical, falling to SlowestSwimScale
	// when the tank is empty.
	PT_API double OxygenSwimScale(const OxygenState& State, const OxygenTuning& Tuning);
}
