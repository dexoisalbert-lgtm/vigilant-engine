--!strict
-- Single tuning surface for the cargo system (docs/GAME_PLAN.md, system 4).
--
-- Cargo "mass" here is thematic (kg-ish numbers on attributes); the flight
-- penalty is ExcessWeightScale * excess published as effective Roblox mass
-- units, which FlightController subtracts from lift exactly like ice weight.
-- The plane root is ~322 mass units; keep effective penalties well under the
-- ~35 studs/s^2 of available thrust-driven climb.

local CargoConfig = {
	-- Overweight.
	CargoCapacityMass = 260, -- kg-ish; default loadout intentionally exceeds this
	ExcessWeightScale = 0.12, -- kg-ish -> effective Roblox mass units

	-- Default loadout (spawned strapped to the hold racks).
	LuggageCount = 6,
	LuggageMass = 15,
	CrateCount = 3,
	CrateMass = 45,
	PianoCount = 1,
	PianoMass = 130,
	ChickenCrateMass = 40,
	ChickenCount = 3,
	CrocCageMass = 60,
	CageEmptyMass = 15, -- a broken cage still weighs something

	-- Physics.
	CargoDensity = 0.3,
	CageBreakImpactSpeed = 26, -- relative impact speed that bursts a cage
	ThrowSpeed = 40, -- luggage-tool throw velocity
	AftPullSpeedPerSecond = 10, -- suction on loose cargo while the ramp is open
	RampWindMinAirspeed = 60,
	DespawnDistanceFromPlane = 150,

	-- Ramp poses (offsets from the plane root; pitch rotates the aft end down).
	RampClosedPosition = Vector3.new(0, 4, 37),
	RampClosedPitchDeg = 90,
	RampOpenPosition = Vector3.new(0, -1.8, 40),
	RampOpenPitchDeg = 35,

	-- Critters.
	ChickenHopMinSeconds = 0.7,
	ChickenHopMaxSeconds = 1.8,
	ChickenHopUpSpeed = 14,
	ChickenHopSideSpeed = 6,
	CatchChickenScore = 5,
	CrocWalkSpeed = 13,
	CrocAggroRange = 20,
	CrocBiteRange = 3.5,
	CrocBiteDamage = 25,
	CrocBiteCooldownSeconds = 2.5,
	CrocKnockdownSeconds = 1.5,
	WrangleScore = 25,
	WranglePromptHoldSeconds = 3,

	-- Accounting loop.
	TickSeconds = 1,
	CritterTickSeconds = 0.35,
}

return CargoConfig
