#include "PaleotrenchCore/Swim.h"

#include <algorithm>
#include <cmath>

namespace pt
{
	namespace
	{
		bool IsFinite(SwimVector V)
		{
			return std::isfinite(V.X) && std::isfinite(V.Y) && std::isfinite(V.Z);
		}

		SwimVector Limit(SwimVector V, double MaxLength)
		{
			if (!IsFinite(V))
			{
				return {};
			}
			// Scale before measuring, so even DBL_MAX input cannot overflow.
			const double Largest = std::max({std::abs(V.X), std::abs(V.Y), std::abs(V.Z)});
			if (Largest > MaxLength)
			{
				V = {V.X / Largest * MaxLength, V.Y / Largest * MaxLength,
					V.Z / Largest * MaxLength};
			}
			const double Length = std::hypot(V.X, V.Y, V.Z);
			if (Length > MaxLength)
			{
				const double Scale = MaxLength / Length;
				V = {V.X * Scale, V.Y * Scale, V.Z * Scale};
			}
			return V;
		}
	}

	bool IsValidSwimTuning(const SwimTuning& T)
	{
		return std::isfinite(T.CruiseSpeed) && T.CruiseSpeed > 0.0 && T.CruiseSpeed <= 2000.0
			&& std::isfinite(T.SprintSpeed) && T.SprintSpeed >= T.CruiseSpeed && T.SprintSpeed <= 4000.0
			&& std::isfinite(T.DragPerSecond) && T.DragPerSecond >= 0.01 && T.DragPerSecond <= 20.0
			&& std::isfinite(T.BuoyancyAcceleration) && std::abs(T.BuoyancyAcceleration) <= 100.0;
	}

	SwimStep IntegrateSwim(SwimVector Velocity, SwimVector Input,
		double DeltaSeconds, bool bSprinting, const SwimTuning& Tuning)
	{
		// Above every valid terminal speed (including maximum buoyancy).
		Velocity = Limit(Velocity, 100000.0);
		if (!std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.0 || !IsValidSwimTuning(Tuning))
		{
			return {Velocity, {}};
		}
		Input = Limit(Input, 1.0);
		const double Dt = std::min(DeltaSeconds, 0.25);
		const double Speed = bSprinting ? Tuning.SprintSpeed : Tuning.CruiseSpeed;
		const SwimVector Target{Input.X * Speed, Input.Y * Speed,
			Input.Z * Speed + Tuning.BuoyancyAcceleration / Tuning.DragPerSecond};
		const double Blend = -std::expm1(-Tuning.DragPerSecond * Dt);
		const double Travel = Blend / Tuning.DragPerSecond;
		return {
			{Velocity.X + (Target.X - Velocity.X) * Blend,
			 Velocity.Y + (Target.Y - Velocity.Y) * Blend,
			 Velocity.Z + (Target.Z - Velocity.Z) * Blend},
			{Target.X * Dt + (Velocity.X - Target.X) * Travel,
			 Target.Y * Dt + (Velocity.Y - Target.Y) * Travel,
			 Target.Z * Dt + (Velocity.Z - Target.Z) * Travel}};
	}
}
