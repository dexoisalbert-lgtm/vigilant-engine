--!strict
-- Supply side of cabin service: builds the service-item Tool templates,
-- hands them out at VE_Dispenser prompts, and owns coffee behavior — sipping
-- grants a caffeination buff, and sipping in freefall off a moving plane is
-- the legendary Coffee Break Plus.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local ServerStorage = game:GetService("ServerStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local CabinConfig = require(Shared:WaitForChild("CabinConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any

local GalleyService = {}
GalleyService.name = "GalleyService"

type HandleSpec = {
	size: Vector3,
	color: Color3,
	cylinder: boolean?,
	uprightGrip: boolean?,
}

local HANDLE_SPECS: { [string]: HandleSpec } = {
	Coffee = {
		size = Vector3.new(1, 0.8, 0.8),
		color = Color3.fromRGB(250, 245, 240),
		cylinder = true,
		uprightGrip = true,
	},
	Soda = {
		size = Vector3.new(0.9, 0.7, 0.7),
		color = Color3.fromRGB(30, 120, 220),
		cylinder = true,
		uprightGrip = true,
	},
	Burger = {
		size = Vector3.new(1, 0.5, 1),
		color = Color3.fromRGB(190, 120, 50),
	},
	Tray = {
		size = Vector3.new(2.4, 0.12, 1.6),
		color = Color3.fromRGB(200, 200, 205),
	},
}

local function buildTemplate(item: string, spec: HandleSpec): Tool
	local tool = Instance.new("Tool")
	tool.Name = item
	tool.CanBeDropped = true
	tool:SetAttribute("Contents", item)

	local handle = Instance.new("Part")
	handle.Name = "Handle"
	handle.Size = spec.size
	handle.Color = spec.color
	handle.Material = Enum.Material.SmoothPlastic
	if spec.cylinder then
		handle.Shape = Enum.PartType.Cylinder
	end
	handle.TopSurface = Enum.SurfaceType.Smooth
	handle.BottomSurface = Enum.SurfaceType.Smooth
	handle.Parent = tool

	if spec.uprightGrip then
		-- Cylinders lie along X; stand cups/cans upright in the hand.
		tool.Grip = CFrame.Angles(0, 0, math.rad(90))
	end
	if item == "Tray" then
		tool.GripPos = Vector3.new(0, 0, -0.8)
	end
	return tool
end

local function countCarriedItems(player: Player): number
	local count = 0
	local backpack = player:FindFirstChild("Backpack")
	if backpack then
		for _, child in backpack:GetChildren() do
			if child:IsA("Tool") and child:GetAttribute("Contents") ~= nil then
				count += 1
			end
		end
	end
	local character = player.Character
	local held = character and character:FindFirstChildOfClass("Tool")
	if held and held:GetAttribute("Contents") ~= nil then
		count += 1
	end
	return count
end

function GalleyService.start()
	local bus = NetBus.server("Cabin")
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model

	local templatesFolder = Instance.new("Folder")
	templatesFolder.Name = "VE_ToolTemplates"
	templatesFolder.Parent = ServerStorage

	local templates: { [string]: Tool } = {}
	for item, spec in HANDLE_SPECS do
		local template = buildTemplate(item, spec)
		template.Parent = templatesFolder
		templates[item] = template
	end

	local function sip(tool: Tool)
		local character = tool.Parent
		if not character or not character:IsA("Model") then
			return
		end
		local player = Players:GetPlayerFromCharacter(character)
		local humanoid = character:FindFirstChildOfClass("Humanoid")
		if not player or not humanoid or humanoid.Health <= 0 then
			return
		end

		local now = os.clock()
		local nextSipAt = tool:GetAttribute("NextSipAt")
		if typeof(nextSipAt) == "number" and now < nextSipAt then
			return
		end
		tool:SetAttribute("NextSipAt", now + CabinConfig.CoffeeSipCooldownSeconds)

		ScoreService.award(player, CabinConfig.CoffeeSipScore)

		-- Caffeination buff; the token invalidates stale resets on re-sip.
		local token = now
		character:SetAttribute("CaffeineToken", token)
		humanoid.WalkSpeed = CabinConfig.CaffeinatedWalkSpeed
		task.delay(CabinConfig.CaffeinatedSeconds, function()
			if character:GetAttribute("CaffeineToken") == token and humanoid.Health > 0 then
				humanoid.WalkSpeed = 16
			end
		end)

		-- Coffee Break Plus: mid-air, unseated, plane still moving. Once per life.
		local airspeed = plane:GetAttribute("Airspeed")
		local flying = typeof(airspeed) == "number"
			and airspeed >= CabinConfig.CoffeeBreakPlusMinAirspeed
		if
			flying
			and humanoid.FloorMaterial == Enum.Material.Air
			and humanoid.SeatPart == nil
			and character:GetAttribute("CoffeeBreakPlus") ~= true
		then
			character:SetAttribute("CoffeeBreakPlus", true)
			ScoreService.award(player, CabinConfig.CoffeeBreakPlusScore)
			bus.fireAll("Toast", {
				text = ("\u{2615} %s took a Coffee Break Plus! +%d"):format(
					player.DisplayName,
					CabinConfig.CoffeeBreakPlusScore
				),
			})
		end

		local sipsLeft = tool:GetAttribute("SipsLeft")
		local remaining = (
			if typeof(sipsLeft) == "number" then sipsLeft else CabinConfig.CoffeeSips
		) - 1
		if remaining <= 0 then
			tool:Destroy()
		else
			tool:SetAttribute("SipsLeft", remaining)
		end
	end

	local function wireTool(tool: Tool)
		if tool:GetAttribute("Contents") == "Coffee" then
			tool.Activated:Connect(function()
				sip(tool)
			end)
		end
	end

	Refs.onAdded(Refs.Tags.Dispenser, function(instance)
		local prompt = instance:FindFirstChildOfClass("ProximityPrompt")
		local item = instance:GetAttribute("Item")
		local template = if typeof(item) == "string" then templates[item] else nil
		if not prompt or not template then
			return
		end
		prompt.Triggered:Connect(function(player)
			if countCarriedItems(player) >= CabinConfig.MaxCarriedItems then
				bus.fire(player, "Toast", { text = "Your hands are full!" })
				return
			end
			local clone = template:Clone()
			wireTool(clone)
			clone.Parent = player:WaitForChild("Backpack")
			local character = player.Character
			local humanoid = character and character:FindFirstChildOfClass("Humanoid")
			if character and humanoid and not character:FindFirstChildOfClass("Tool") then
				humanoid:EquipTool(clone)
			end
		end)
	end)
end

return GalleyService
