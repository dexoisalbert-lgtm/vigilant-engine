--!strict
-- Single tuning surface for the flight model. Every magic number the
-- FlightController uses lives here so flight feel is adjustable in one place.
-- Units: studs, seconds, degrees (converted to radians at the use site), kg.

local FlightConfig = {
	-- Thrust / speed. Thrust is expressed as acceleration so it is independent
	-- of assembly mass; FlightController multiplies by AssemblyMass.
	MaxForwardAccel = 35, -- studs/s^2 at full throttle, all engines healthy
	MaxSpeed = 300, -- terminal speed at full throttle (sets drag coefficient)
	StallSpeed = 80, -- below this, lift fades linearly and the plane sinks

	-- Attitude control (per-second rates at full stick deflection).
	PitchRateDeg = 40,
	RollRateDeg = 80,
	MaxPitchDeg = 40,
	MaxRollDeg = 55,
	TurnRateDegAtFullBank = 20, -- yaw rate when fully banked
	LevelDecayDeg = 5, -- auto-level rate when the stick is centered

	-- Ground handling.
	GroundSteerRateDeg = 30, -- nosewheel steering yaw rate (roll stick on ground)
	RotateSpeed = 65, -- minimum ground speed before pitch-up is allowed
	GroundProbeDepth = 3, -- raycast length below the root part for ground checks

	-- Engine / fuel.
	ThrottleChangeRate = 0.4, -- throttle fraction per second (Shift/Ctrl ramp)
	FuelCapacityKg = 4000,
	FuelBurnKgPerSec = 3, -- at full throttle; scales linearly with throttle

	-- Autopilot (simple altitude + wings-level hold).
	AutopilotMaxPitchDeg = 10,
	AutopilotAltGain = 0.01, -- pitch command per stud of altitude error
	AutopilotVsGain = 0.02, -- pitch damping per stud/s of vertical speed

	-- Networking.
	InputSendHz = 20, -- client input send rate cap
	InputKeepaliveSeconds = 0.25, -- client re-sends unchanged input this often
	InputTimeoutSeconds = 0.6, -- server zeroes input if nothing arrives for this long

	-- Instruments.
	InstrumentsUpdateSeconds = 0.1,
	LowFuelFraction = 0.15,
}

return FlightConfig
