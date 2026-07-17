--!strict
-- DamageService — Phase 3 (docs/GAME_PLAN.md, system 3: Emergency Maintenance).
-- Will own the Damage/State/RepairKind attributes on every VE_Repairable part,
-- run the escalation loop (crack -> depressurization, fire -> engine loss,
-- ice -> weight), schedule lightning strikes, and handle the "Repair" NetBus
-- channel for blowtorch/hammer/extinguisher/ice-axe interactions.

local DamageService = {}
DamageService.name = "DamageService"

function DamageService.start()
	-- Stub: implemented in Phase 3.
end

return DamageService
