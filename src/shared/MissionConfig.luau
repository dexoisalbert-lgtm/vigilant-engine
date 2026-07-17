--!strict
-- Single tuning surface for missions (docs/GAME_PLAN.md, system 6 + rounds).
-- Mission definitions currently live here as data; per-map mission modules
-- (src/server/missions/) arrive when additional maps do.

local MissionConfig = {
	IntermissionSeconds = 30,
	EndedHoldSeconds = 6,
	MissionWinScore = 50,
	Rotation = { "Delivery", "Smuggle", "Defend" },

	Definitions = {
		Delivery = {
			name = "Milk Run",
			objective = "Fly to the beacon airfield and land safely.",
			durationSeconds = 420,
		},
		Smuggle = {
			name = "Special Cargo",
			objective = "Drop packages on the smuggler zones, then land back home. Mind the marshal.",
			durationSeconds = 480,
			packageCount = 4,
			zoneCount = 3,
		},
		Defend = {
			name = "Hostile Skies",
			objective = "Survive the fighter waves, then land anywhere.",
			durationSeconds = 480,
			waves = 3,
			waveSize = 2,
		},
	},

	-- Flight-state thresholds.
	TakeoffAirspeed = 80,
	LandingAirspeed = 15,
	LandingDwellSeconds = 3,
	CrashVerticalSpeed = 60, -- touching down harder than this sinks the mission

	-- Delivery.
	DeliveryDistanceMin = 2500,
	DeliveryDistanceMax = 3500,
	DeliveryWinRadius = 200,

	-- Smuggle.
	ZoneDistanceMin = 800,
	ZoneDistanceMax = 2000,
	DropZoneRadius = 16,
	PackageLostSeconds = 8, -- landed outside a zone this long = lost
	MarshalDetectRadius = 7,
	MarshalCuffSeconds = 15,
	MarshalWalkSpeed = 10,

	-- Home pad (Smuggle return + generic landings) is the runway itself.
	HomeRadius = 300,
}

return MissionConfig
