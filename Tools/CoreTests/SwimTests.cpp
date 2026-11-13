#include "TestFramework.h"
#include "PaleotrenchCore/Swim.h"

#include <cmath>
#include <limits>

namespace
{
	bool Near(double A, double B, double Epsilon = 1e-8)
	{
		return std::abs(A - B) <= Epsilon;
	}

	double Length(pt::SwimVector V)
	{
		return std::hypot(V.X, V.Y, V.Z);
	}

	pt::SwimStep Simulate(int Hz, pt::SwimVector Input, bool bSprint = false,
		pt::SwimTuning Tuning = {})
	{
		pt::SwimStep Result{};
		for (int I = 0; I < Hz * 10; ++I)
		{
			const auto Step = pt::IntegrateSwim(Result.Velocity, Input, 1.0 / Hz, bSprint, Tuning);
			Result.Velocity = Step.Velocity;
			Result.Displacement.X += Step.Displacement.X;
			Result.Displacement.Y += Step.Displacement.Y;
			Result.Displacement.Z += Step.Displacement.Z;
		}
		return Result;
	}
}

PT_TEST(Swim_NeutralBuoyancyHovers)
{
	const auto Result = Simulate(60, {});
	PT_EXPECT(Length(Result.Velocity) == 0.0);
	PT_EXPECT(Length(Result.Displacement) == 0.0);
}

PT_TEST(Swim_AllSixDirectionsReachCruiseSpeed)
{
	const pt::SwimVector Directions[] = {
		{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
	for (const auto Direction : Directions)
	{
		const auto Result = Simulate(60, Direction);
		PT_EXPECT(Near(Length(Result.Velocity), 240.0, 1e-5));
		PT_EXPECT(Result.Displacement.X * Direction.X + Result.Displacement.Y * Direction.Y
			+ Result.Displacement.Z * Direction.Z > 2000.0);
	}
}

PT_TEST(Swim_DiagonalsShareOneSpeedBudget)
{
	const auto Forward = Simulate(60, {1, 0, 0});
	const auto Diagonal = Simulate(60, {1, 1, 1});
	PT_EXPECT(Near(Length(Forward.Velocity), Length(Diagonal.Velocity)));
	PT_EXPECT(Near(Length(Forward.Displacement), Length(Diagonal.Displacement)));
}

PT_TEST(Swim_AnalogueInputPreservesSlowMovement)
{
	const auto Full = Simulate(60, {1, 0, 0});
	const auto Quarter = Simulate(60, {0.25, 0, 0});
	PT_EXPECT(Near(Full.Velocity.X * 0.25, Quarter.Velocity.X));
	PT_EXPECT(Near(Full.Displacement.X * 0.25, Quarter.Displacement.X));
}

PT_TEST(Swim_FrameRateIndependentAt30_60_144Hz)
{
	const auto A = Simulate(30, {0.7, -0.3, 0.4}, true);
	for (const int Hz : {60, 144})
	{
		const auto B = Simulate(Hz, {0.7, -0.3, 0.4}, true);
		PT_EXPECT(Near(A.Velocity.X, B.Velocity.X));
		PT_EXPECT(Near(A.Displacement.X, B.Displacement.X));
		PT_EXPECT(Near(A.Displacement.Y, B.Displacement.Y));
		PT_EXPECT(Near(A.Displacement.Z, B.Displacement.Z));
	}
}

PT_TEST(Swim_ReleaseCoastsThenStopsWithoutReversing)
{
	pt::SwimVector Velocity{240, -120, 60};
	double Previous = Length(Velocity);
	for (int I = 0; I < 600; ++I)
	{
		const auto Step = pt::IntegrateSwim(Velocity, {}, 1.0 / 60.0, false, {});
		PT_EXPECT(Length(Step.Velocity) < Previous);
		PT_EXPECT(Step.Velocity.X > 0 && Step.Velocity.Y < 0 && Step.Velocity.Z > 0);
		PT_EXPECT(Step.Displacement.X > 0);
		Velocity = Step.Velocity;
		Previous = Length(Velocity);
	}
	PT_EXPECT(Length(Velocity) < 0.001);
}

PT_TEST(Swim_ReversalTakesTime)
{
	const auto First = pt::IntegrateSwim({240, 0, 0}, {-1, 0, 0}, 1.0 / 60.0, false, {});
	PT_EXPECT(First.Velocity.X > 0.0 && First.Velocity.X < 240.0);
	const auto Sprint = Simulate(60, {1, 0, 0}, true);
	PT_EXPECT(Near(Sprint.Velocity.X, 420.0, 1e-5));
	const auto Released = pt::IntegrateSwim(Sprint.Velocity, {1, 0, 0}, 0.1, false, {});
	PT_EXPECT(Released.Velocity.X > 240.0 && Released.Velocity.X < Sprint.Velocity.X);
}

PT_TEST(Swim_BuoyancyActsAlongWorldUp)
{
	pt::SwimTuning Tuning;
	Tuning.BuoyancyAcceleration = 20.0;
	const auto Result = Simulate(60, {}, false, Tuning);
	PT_EXPECT(Result.Velocity.X == 0.0 && Result.Velocity.Y == 0.0);
	PT_EXPECT(Near(Result.Velocity.Z, 10.0, 1e-6));
	Tuning.BuoyancyAcceleration = -20.0;
	PT_EXPECT(Near(Simulate(60, {}, false, Tuning).Velocity.Z, -10.0, 1e-6));
}

PT_TEST(Swim_InvalidTimeDoesNotMoveAndHitchesAreBounded)
{
	for (const double Dt : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
		std::numeric_limits<double>::infinity()})
	{
		const auto Result = pt::IntegrateSwim({240, 0, 0}, {1, 0, 0}, Dt, false, {});
		PT_EXPECT(Length(Result.Displacement) == 0.0);
	}
	const auto Hitch = pt::IntegrateSwim({}, {1, 0, 0}, 10.0, false, {});
	const auto Capped = pt::IntegrateSwim({}, {1, 0, 0}, 0.25, false, {});
	PT_EXPECT(Near(Hitch.Displacement.X, Capped.Displacement.X));
}

PT_TEST(Swim_ExtremeValidTuningKeepsItsAnalyticTrajectory)
{
	const pt::SwimTuning Tuning{2000.0, 4000.0, 0.01, 100.0};
	PT_EXPECT(pt::IsValidSwimTuning(Tuning));
	pt::SwimVector Velocity{};
	for (int I = 0; I < 2000; ++I)
	{
		Velocity = pt::IntegrateSwim(Velocity, {0, 0, 1}, 0.25, true, Tuning).Velocity;
	}
	// Upward propulsion plus buoyancy has a 14000 cm/s terminal speed.
	PT_EXPECT(Near(Velocity.Z, 14000.0 * (1.0 - std::exp(-5.0)), 1e-7));
}

PT_TEST(Swim_InvalidVectorsAndTuningCannotProduceNaNs)
{
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const auto Invalid = pt::IntegrateSwim({NaN, 0, 0}, {0, NaN, 0}, 0.1, false, {});
	PT_EXPECT(Length(Invalid.Velocity) == 0.0);
	const double Huge = std::numeric_limits<double>::max();
	const auto Large = pt::IntegrateSwim({Huge, Huge, Huge}, {Huge, Huge, Huge}, 0.1, true, {});
	PT_EXPECT(std::isfinite(Length(Large.Velocity)));
	PT_EXPECT(std::isfinite(Length(Large.Displacement)));
	pt::SwimTuning Tuning;
	Tuning.DragPerSecond = 0.0;
	PT_EXPECT(!pt::IsValidSwimTuning(Tuning));
	PT_EXPECT(Length(pt::IntegrateSwim({}, {1, 0, 0}, 0.1, false, Tuning).Displacement) == 0.0);
	Tuning = {};
	Tuning.SprintSpeed = 100.0;
	PT_EXPECT(!pt::IsValidSwimTuning(Tuning));
	Tuning = {};
	Tuning.BuoyancyAcceleration = NaN;
	PT_EXPECT(!pt::IsValidSwimTuning(Tuning));
}
