--!strict
-- Builds the Phase 1 test map: flat ground, a runway strip with centerline
-- stripes, and a spawn pad. Returns the recommended plane spawn CFrame
-- (on the runway threshold, facing down the strip toward -Z).

local CollectionService = game:GetService("CollectionService")

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local Refs = require(ReplicatedStorage:WaitForChild("VEShared"):WaitForChild("Refs"))

local RunwayBuilder = {}

local function basePart(name: string, size: Vector3, cframe: CFrame, color: Color3): Part
	local part = Instance.new("Part")
	part.Name = name
	part.Size = size
	part.CFrame = cframe
	part.Color = color
	part.Anchored = true
	part.TopSurface = Enum.SurfaceType.Smooth
	part.BottomSurface = Enum.SurfaceType.Smooth
	return part
end

function RunwayBuilder.build(): CFrame
	local map = Instance.new("Model")
	map.Name = "VE_Map"

	local ground = basePart(
		"Ground",
		Vector3.new(2048, 4, 2048),
		CFrame.new(0, -2, 0),
		Color3.fromRGB(88, 133, 68)
	)
	ground.Material = Enum.Material.Grass
	ground.Parent = map

	-- Runway top surface sits at y = 0.5, running along the Z axis.
	local runway =
		basePart("Runway", Vector3.new(36, 1, 700), CFrame.new(0, 0, 0), Color3.fromRGB(80, 80, 84))
	runway.Material = Enum.Material.Concrete
	CollectionService:AddTag(runway, Refs.Tags.Runway)
	runway.Parent = map

	for index = 0, 13 do
		local stripe = basePart(
			"CenterlineStripe",
			Vector3.new(2, 0.1, 14),
			CFrame.new(0, 0.55, -312 + index * 48),
			Color3.fromRGB(240, 240, 240)
		)
		stripe.Material = Enum.Material.Concrete
		stripe.Parent = map
	end

	local spawnPad = Instance.new("SpawnLocation")
	spawnPad.Name = "SpawnPad"
	spawnPad.Size = Vector3.new(16, 1, 16)
	spawnPad.CFrame = CFrame.new(28, 1, 205)
	spawnPad.Color = Color3.fromRGB(120, 120, 126)
	spawnPad.Anchored = true
	spawnPad.Neutral = true
	spawnPad.Duration = 0
	spawnPad.TopSurface = Enum.SurfaceType.Smooth
	spawnPad.Parent = map

	map.Parent = workspace

	-- Threshold at z = 200 leaves ~550 studs of takeoff run toward -Z.
	return CFrame.new(0, 2.55, 200)
end

return RunwayBuilder
