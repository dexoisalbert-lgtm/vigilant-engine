--!strict
-- Client entry point. Starts every controller in its own task so one
-- controller's blocking boot (or failure) can't stall the others.
--
-- Deployed as: StarterPlayer/StarterPlayerScripts/VEClient/Main (LocalScript)
-- Sibling:     VEClient/Controllers (Folder)

local controllersFolder = script.Parent:WaitForChild("Controllers")

local controllerModules = {
	controllersFolder:WaitForChild("PilotController"),
	controllersFolder:WaitForChild("PilotCameraController"),
	controllersFolder:WaitForChild("InstrumentsController"),
	controllersFolder:WaitForChild("CabinController"),
	controllersFolder:WaitForChild("GunnerController"),
	controllersFolder:WaitForChild("WingWalkController"),
	controllersFolder:WaitForChild("MissionController"),
}

for _, moduleScript in controllerModules do
	local controller = require(moduleScript) :: any
	task.spawn(function()
		local ok, err = pcall(controller.start)
		if not ok then
			warn(("[VE] %s failed: %s"):format(controller.name or moduleScript.Name, tostring(err)))
		end
	end)
end

print("[VE] client booted")
