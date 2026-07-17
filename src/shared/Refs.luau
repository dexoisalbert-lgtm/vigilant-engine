--!strict
-- Tag-based instance locator. All world lookups go through here so scripts can
-- be authored before the world exists and still bind correctly once deployed.
-- The builders in src/server/setup/ are responsible for applying these tags.

local CollectionService = game:GetService("CollectionService")

local Refs = {}

Refs.Tags = {
	Plane = "VE_Plane",
	PlanePrimary = "VE_PlanePrimary",
	PilotSeat = "VE_PilotSeat",
	Engine = "VE_Engine",
	CockpitDash = "VE_CockpitDash",
	CockpitWindow = "VE_CockpitWindow",
	Runway = "VE_Runway",
	CabinSeat = "VE_CabinSeat",
	Dispenser = "VE_Dispenser",
	CargoHold = "VE_CargoHold",
	CargoRamp = "VE_CargoRamp",
	CargoItem = "VE_CargoItem",
	Wing = "VE_Wing",
	GatlingSeat = "VE_GatlingSeat",
	DropZone = "VE_DropZone",
	SmugglerHold = "VE_SmugglerHold",
	Repairable = "VE_Repairable",
}

-- Returns the first tagged instance, or nil.
function Refs.find(tag: string): Instance?
	return CollectionService:GetTagged(tag)[1]
end

-- Returns the first tagged instance, polling until one exists or the timeout
-- elapses (boot-time lookups only; not for per-frame use).
function Refs.waitFor(tag: string, timeout: number?): Instance
	local deadline = os.clock() + (timeout or 60)
	while os.clock() < deadline do
		local found = Refs.find(tag)
		if found then
			return found
		end
		task.wait(0.1)
	end
	error(("Refs.waitFor(%q) timed out"):format(tag))
end

function Refs.all(tag: string): { Instance }
	return CollectionService:GetTagged(tag)
end

-- Runs callback for every current and future instance carrying the tag.
function Refs.onAdded(tag: string, callback: (Instance) -> ()): RBXScriptConnection
	for _, instance in CollectionService:GetTagged(tag) do
		task.spawn(callback, instance)
	end
	return CollectionService:GetInstanceAddedSignal(tag):Connect(callback)
end

return Refs
