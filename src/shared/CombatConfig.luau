--!strict
-- Single tuning surface for wing-walking combat (docs/GAME_PLAN.md, system 5).

local CombatConfig = {
	-- Wing gatlings. Client streams aim direction; server does hitreg with a
	-- forgiveness cone (dot threshold ~= 10 degrees) against fighters.
	GatlingFireCooldownSeconds = 0.12,
	GatlingRange = 600,
	GatlingDamage = 10,
	GatlingForgivenessCos = 0.985,
	FighterKillScore = 30,

	-- Hostile fighters (kinematic AI planes orbiting the airliner).
	FighterHealth = 60,
	FighterSpeed = 150, -- studs/s
	FighterTurnResponsiveness = 1.2,
	FighterOrbitRadius = 190,
	FighterOrbitHeight = 25,
	FighterOrbitAngularSpeed = 0.35, -- rad/s strafe circling
	FighterAttackRange = 350,
	FighterBoltRange = 600,
	FighterBoltPlayerDamage = 15,
	FighterBoltCrackChance = 0.45, -- chance a bolt that hits hull/engine damages it
	FighterBoltsPerBurst = 3,
	FighterBurstMinSeconds = 2.5,
	FighterBurstMaxSeconds = 5,

	-- Wave spawner.
	FighterSpawnMinSeconds = 90,
	FighterSpawnMaxSeconds = 180,
	FighterMaxActive = 3,
	FighterSpawnDistance = 700,
	FighterMinPlaneAirspeed = 60, -- no spawns while slow/parked
	FighterDisengageAirspeed = 40, -- fighters break off when the plane slows
	FighterLifetimeSeconds = 240,

	-- Wing-walk harness: how strongly the client carries the character along
	-- with the plane's motion while standing on its exterior. 0 = native
	-- physics only, 1 = hard-locked to the airframe. Tune at playtest; drop
	-- it if characters overshoot (double-carry) on straight flight.
	WingStickStrength = 0.5,
}

return CombatConfig
