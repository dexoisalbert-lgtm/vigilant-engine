--!strict
-- Single tuning surface for the damage/repair system (docs/GAME_PLAN.md,
-- system 3). Damage lifecycle per part:
--   Hull:   Ok -> Cracked (weld it) -> unrepaired too long -> Detached (hammer)
--   Engine: Ok -> OnFire (extinguish; health drains) -> health 0 -> Detached
--   Wing:   Ok -> Iced (ice axe; adds virtual weight the wings must carry)

local DamageConfig = {
	-- Hazard scheduler (replaced by mission scripts in Phase 6).
	MinAirspeedForHazards = 60,
	HazardMinIntervalSeconds = 40,
	HazardMaxIntervalSeconds = 90,
	EventWeights = {
		HullCrack = 40,
		EngineFire = 25,
		Lightning = 20,
		Icing = 15,
	},
	LightningEngineFireChance = 0.4,

	-- Damage points per state (repair tools subtract RepairPower per hold).
	CrackDamage = 60,
	DetachDamage = 100,
	FireDamage = 60,
	IceDamage = 100,

	-- Escalation.
	EscalationTickSeconds = 1,
	CrackWorsenSeconds = 45, -- cracked hull tears off after this long unrepaired
	FireBurnPerSecond = 3, -- engine health drain while on fire
	EngineRegenPerSecond = 2, -- slow recovery while Ok
	RestoredEngineHealth = 30, -- hammering a detached engine rebuilds to this
	DetachFlingSpeed = 55, -- studs/s given to the torn-off panel clone

	-- Depressurization pull on loose characters near a breach.
	DepressurizationRadius = 14,
	CrackedPullSpeedPerSecond = 6, -- velocity gain per second toward the hole
	DetachedPullSpeedPerSecond = 16,

	-- Repairs.
	RepairPower = {
		Weld = 34,
		Hammer = 50,
		Extinguisher = 40,
		IceAxe = 34,
	},
	RepairScore = 15,
	RepairPromptHoldSeconds = 0.8,

	-- Ice weight in effective Roblox mass units (the plane root is ~320, and
	-- lift loses IceMass * gravity). Both wings iced = ~16 studs/s^2 of sink
	-- to fight — serious but recoverable with full throttle and pitch.
	IceMassPerIcedPart = 15,
}

return DamageConfig
