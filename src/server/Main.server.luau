--!strict
-- Server entry point. Builds the world if this place doesn't already contain
-- one (tag-checked, so a hand-built place in Studio wins over the builders),
-- then starts every service.
--
-- Deployed as: ServerScriptService/VEServer/Main (Script)
-- Siblings:    VEServer/Services (Folder), VEServer/Setup (Folder)

local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))

local serverFolder = script.Parent :: Folder
local servicesFolder = serverFolder:WaitForChild("Services")
local setupFolder = serverFolder:WaitForChild("Setup")

if not Refs.find(Refs.Tags.Plane) then
	local RunwayBuilder = require(setupFolder:WaitForChild("RunwayBuilder")) :: any
	local PlaneRig = require(setupFolder:WaitForChild("PlaneRig")) :: any
	local planeSpawnCf = RunwayBuilder.build()
	PlaneRig.build(planeSpawnCf)
	print("[VE] world built by setup scripts")
end

local serviceModules = {
	servicesFolder:WaitForChild("ScoreService"),
	servicesFolder:WaitForChild("StatsService"),
	servicesFolder:WaitForChild("FlightController"),
	servicesFolder:WaitForChild("GalleyService"),
	servicesFolder:WaitForChild("PassengerService"),
	servicesFolder:WaitForChild("DamageService"),
	servicesFolder:WaitForChild("CargoService"),
	servicesFolder:WaitForChild("CombatService"),
	servicesFolder:WaitForChild("MissionService"),
}

for _, moduleScript in serviceModules do
	local service = require(moduleScript) :: any
	local ok, err = pcall(service.start)
	if ok then
		print(("[VE] %s started"):format(service.name or moduleScript.Name))
	else
		warn(
			("[VE] %s failed to start: %s"):format(service.name or moduleScript.Name, tostring(err))
		)
	end
end

print("[VE] server booted")
