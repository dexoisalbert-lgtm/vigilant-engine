#pragma once

#include "PaleotrenchCore/Api.h"

namespace pt
{
	// World-space centimetres and seconds, matching Unreal. No engine types.
	struct SwimVector
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
	};

	struct SwimTuning
	{
		double CruiseSpeed = 240.0;
		double SprintSpeed = 420.0;
		double DragPerSecond = 2.0;
		double BuoyancyAcceleration = 0.0; // positive rises along world Z
	};

	struct SwimStep
	{
		SwimVector Velocity;
		SwimVector Displacement;
	};

	PT_API bool IsValidSwimTuning(const SwimTuning& Tuning);

	// Exact integration of dv/dt = drag * (input * speed - v) + buoyancy.
	// Input is a world-space analogue vector, limited to unit length. All axes
	// share the same speed budget. Releasing input coasts to rest; no instant stop.
	// A hitch advances at most 0.25 s. Invalid time/settings produce no movement;
	// non-finite input/velocity are treated as zero, preventing NaN transforms.
	PT_API SwimStep IntegrateSwim(SwimVector Velocity, SwimVector Input,
		double DeltaSeconds, bool bSprinting, const SwimTuning& Tuning);
}
