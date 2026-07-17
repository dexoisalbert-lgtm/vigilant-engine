--!strict
-- Shared type definitions. Import with:
--   local Types = require(ReplicatedStorage.VEShared.Types)

-- Pilot control input, sent client -> server over the "Cockpit" NetBus channel.
-- p: pitch stick, +1 = pull up. r: roll stick, +1 = bank right.
export type PilotInput = {
	p: number,
	r: number,
}

-- Damage model (see docs/GAME_PLAN.md, system 3).
export type DamageState = "Ok" | "Cracked" | "OnFire" | "Iced" | "Detached"
export type RepairKind = "Weld" | "Hammer" | "Extinguisher" | "IceAxe"

-- Mission flow (see docs/GAME_PLAN.md, MissionService).
export type MissionPhase = "Boarding" | "Takeoff" | "Cruise" | "Crisis" | "Landing" | "Ended"

export type Role = "Pilot" | "Crew"

-- Every server service module exposes this shape; Main.server calls start().
export type Service = {
	name: string,
	start: () -> (),
}

return {}
