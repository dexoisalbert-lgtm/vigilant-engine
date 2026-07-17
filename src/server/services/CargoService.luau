--!strict
-- CargoService — Phase 4 (docs/GAME_PLAN.md, system 4: Cargo & Baggage).
-- Spawns the default loadout strapped into the hold, runs the cargo ramp,
-- strap/unstrap prompts, overweight accounting, jettison suction, ejected
-- cargo despawning, and the animal cargo (chickens and the crocodile).
--
-- Straps are WeldConstraints to the hold floor: strapped cargo is part of the
-- plane assembly; unstrapped cargo is loose physics that slides with pitch,
-- gets sucked aft while the ramp is open, and can be shoved out by hand.
-- Luggage pieces are real Tool instances lying in the world: touch to pick
-- up, click to hurl them (out the ramp, ideally).
--
-- Overweight: cargo carries thematic CargoKg attributes; the amount over
-- CargoCapacityMass is scaled and published as ExcessCargoMass, which
-- FlightController subtracts from lift. Jettison enough and the plane climbs.
--
-- Cages burst on hard impacts (slides, drops, crash landings), freeing their
-- animals: chickens hop around getting underfoot until caught; the crocodile
-- hunts the nearest player or passenger and knocks them flat.

local CollectionService = game:GetService("CollectionService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local CargoConfig = require(Shared:WaitForChild("CargoConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any

local CargoService = {}
CargoService.name = "CargoService"

local RNG = Random.new()

local EMOJI_BOX = "\u{1F4E6}"
local EMOJI_CHICKEN = "\u{1F414}"
local EMOJI_CROC = "\u{1F40A}"

local LUGGAGE_COLORS = {
	Color3.fromRGB(148, 60, 44),
	Color3.fromRGB(52, 87, 132),
	Color3.fromRGB(75, 110, 65),
	Color3.fromRGB(120, 90, 140),
	Color3.fromRGB(60, 60, 66),
}

type CargoRecord = {
	part: BasePart,
	class: string,
	strapWeld: WeldConstraint?,
	strapPrompt: ProximityPrompt,
	broken: boolean,
}

type Critter = {
	model: Model,
	root: BasePart,
	humanoid: Humanoid?,
	kind: string, -- "Chicken" | "Croc"
	nextActAt: number,
}

function CargoService.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local planeRoot = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart
	local holdFloor = Refs.waitFor(Refs.Tags.CargoHold) :: BasePart
	local ramp = Refs.waitFor(Refs.Tags.CargoRamp) :: BasePart
	local bus = NetBus.server("Cabin")

	local cargoItems: { CargoRecord } = {}
	local luggageTools: { Tool } = {}
	local critters: { Critter } = {}

	local cargoFolder = Instance.new("Folder")
	cargoFolder.Name = "VE_Cargo"
	cargoFolder.Parent = workspace

	local function toastAll(text: string)
		bus.fireAll("Toast", { text = text })
	end

	local function readNumber(instance: Instance, name: string, fallback: number): number
		local value = instance:GetAttribute(name)
		return if typeof(value) == "number" then value else fallback
	end

	local function relToPlane(position: Vector3): Vector3
		return planeRoot.CFrame:PointToObjectSpace(position)
	end

	local function withinPlane(position: Vector3): boolean
		local rel = relToPlane(position)
		return math.abs(rel.X) <= 6 and rel.Y >= -3 and rel.Y <= 10 and rel.Z >= -24 and rel.Z <= 39
	end

	local function withinHold(position: Vector3): boolean
		local rel = relToPlane(position)
		return math.abs(rel.X) <= 5 and rel.Y >= -1 and rel.Y <= 9 and rel.Z >= 23 and rel.Z <= 38
	end

	-- ============================== Ramp ==============================

	local rampWeld: WeldConstraint? = ramp:FindFirstChildOfClass("WeldConstraint")
	local rampPrompt = ramp:FindFirstChildOfClass("ProximityPrompt")

	local function setRamp(open: boolean)
		plane:SetAttribute("RampOpen", open)
		local position = if open
			then CargoConfig.RampOpenPosition
			else CargoConfig.RampClosedPosition
		local pitch = if open then CargoConfig.RampOpenPitchDeg else CargoConfig.RampClosedPitchDeg
		if rampWeld then
			rampWeld:Destroy()
		end
		ramp.CFrame = planeRoot.CFrame * CFrame.new(position) * CFrame.Angles(math.rad(pitch), 0, 0)
		local weld = Instance.new("WeldConstraint")
		weld.Part0 = planeRoot
		weld.Part1 = ramp
		weld.Parent = ramp
		rampWeld = weld
		if rampPrompt then
			rampPrompt.ActionText = if open then "Close Ramp" else "Open Ramp"
		end
		toastAll(
			if open
				then EMOJI_BOX .. " Cargo ramp open — everything loose is going out the back!"
				else EMOJI_BOX .. " Cargo ramp closed."
		)
	end

	if rampPrompt then
		rampPrompt.Triggered:Connect(function()
			setRamp(plane:GetAttribute("RampOpen") ~= true)
		end)
	end

	-- ============================== Straps ==============================

	local function strapDown(record: CargoRecord)
		if record.strapWeld then
			return
		end
		local weld = Instance.new("WeldConstraint")
		weld.Part0 = holdFloor
		weld.Part1 = record.part
		weld.Parent = record.part
		record.strapWeld = weld
		record.strapPrompt.ActionText = "Unstrap"
	end

	local function unstrap(record: CargoRecord)
		if record.strapWeld then
			record.strapWeld:Destroy()
			record.strapWeld = nil
		end
		record.strapPrompt.ActionText = "Strap Down"
	end

	-- ============================== Critters ==============================

	local function collectBiteTargets(): { { humanoid: Humanoid, root: BasePart } }
		local targets = {}
		local function add(character: Model)
			local humanoid = character:FindFirstChildOfClass("Humanoid")
			local root = character:FindFirstChild("HumanoidRootPart")
			if humanoid and humanoid.Health > 0 and root and root:IsA("BasePart") then
				table.insert(targets, { humanoid = humanoid, root = root })
			end
		end
		for _, player in Players:GetPlayers() do
			if player.Character then
				add(player.Character)
			end
		end
		local passengersFolder = workspace:FindFirstChild("VE_Passengers")
		if passengersFolder then
			for _, child in passengersFolder:GetChildren() do
				if child:IsA("Model") then
					add(child)
				end
			end
		end
		return targets
	end

	local function spawnChicken(worldPosition: Vector3)
		local model = Instance.new("Model")
		model.Name = "Chicken"
		local body = Instance.new("Part")
		body.Name = "Body"
		body.Size = Vector3.new(1.2, 1, 1.6)
		body.Color = Color3.fromRGB(245, 243, 235)
		body.Material = Enum.Material.SmoothPlastic
		body.CustomPhysicalProperties = PhysicalProperties.new(0.4, 0.4, 0.3)
		body.CFrame = CFrame.new(worldPosition)
		body.Parent = model

		local comb = Instance.new("Part")
		comb.Name = "Comb"
		comb.Size = Vector3.new(0.3, 0.3, 0.5)
		comb.Color = Color3.fromRGB(196, 40, 28)
		comb.Material = Enum.Material.SmoothPlastic
		comb.Massless = true
		comb.CFrame = body.CFrame * CFrame.new(0, 0.65, -0.4)
		comb.Parent = model
		local weld = Instance.new("WeldConstraint")
		weld.Part0 = body
		weld.Part1 = comb
		weld.Parent = comb

		local catchPrompt = Instance.new("ProximityPrompt")
		catchPrompt.Name = "CatchPrompt"
		catchPrompt.ActionText = "Catch Chicken"
		catchPrompt.ObjectText = "Chicken"
		catchPrompt.HoldDuration = 0.5
		catchPrompt.MaxActivationDistance = 6
		catchPrompt.RequiresLineOfSight = false
		catchPrompt.Parent = body

		model.PrimaryPart = body
		model.Parent = cargoFolder

		local critter: Critter = {
			model = model,
			root = body,
			humanoid = nil,
			kind = "Chicken",
			nextActAt = 0,
		}
		table.insert(critters, critter)

		catchPrompt.Triggered:Connect(function(player)
			if not model.Parent then
				return
			end
			ScoreService.award(player, CargoConfig.CatchChickenScore)
			bus.fire(player, "Toast", {
				text = ("%s +%d  Chicken caught!"):format(
					EMOJI_CHICKEN,
					CargoConfig.CatchChickenScore
				),
			})
			model:Destroy()
		end)
	end

	local function spawnCroc(worldPosition: Vector3)
		local model = Instance.new("Model")
		model.Name = "Crocodile"

		local body = Instance.new("Part")
		body.Name = "HumanoidRootPart"
		body.Size = Vector3.new(1.8, 1.2, 4.5)
		body.Color = Color3.fromRGB(58, 105, 52)
		body.Material = Enum.Material.SmoothPlastic
		body.CFrame = CFrame.new(worldPosition + Vector3.new(0, 1, 0))
		body.Parent = model

		local snout = Instance.new("Part")
		snout.Name = "Snout"
		snout.Size = Vector3.new(1.1, 0.7, 1.6)
		snout.Color = Color3.fromRGB(72, 125, 62)
		snout.Material = Enum.Material.SmoothPlastic
		snout.Massless = true
		snout.CFrame = body.CFrame * CFrame.new(0, -0.1, -3)
		snout.Parent = model
		local snoutWeld = Instance.new("WeldConstraint")
		snoutWeld.Part0 = body
		snoutWeld.Part1 = snout
		snoutWeld.Parent = snout

		local tail = Instance.new("Part")
		tail.Name = "Tail"
		tail.Size = Vector3.new(1.2, 0.7, 2.2)
		tail.Color = Color3.fromRGB(58, 105, 52)
		tail.Material = Enum.Material.SmoothPlastic
		tail.Massless = true
		tail.CFrame = body.CFrame * CFrame.new(0, -0.1, 3.3)
		tail.Parent = model
		local tailWeld = Instance.new("WeldConstraint")
		tailWeld.Part0 = body
		tailWeld.Part1 = tail
		tailWeld.Parent = tail

		local humanoid = Instance.new("Humanoid")
		humanoid.RequiresNeck = false
		humanoid.HipHeight = 0.4
		humanoid.WalkSpeed = CargoConfig.CrocWalkSpeed
		humanoid.DisplayName = "Crocodile"
		humanoid.Parent = model

		local wranglePrompt = Instance.new("ProximityPrompt")
		wranglePrompt.Name = "WranglePrompt"
		wranglePrompt.ActionText = "Wrangle Croc"
		wranglePrompt.ObjectText = "Crocodile"
		wranglePrompt.HoldDuration = CargoConfig.WranglePromptHoldSeconds
		wranglePrompt.MaxActivationDistance = 7
		wranglePrompt.RequiresLineOfSight = false
		wranglePrompt.Parent = body

		model.PrimaryPart = body
		model.Parent = cargoFolder
		pcall(function()
			body:SetNetworkOwner(nil)
		end)

		local critter: Critter = {
			model = model,
			root = body,
			humanoid = humanoid,
			kind = "Croc",
			nextActAt = 0,
		}
		table.insert(critters, critter)

		wranglePrompt.Triggered:Connect(function(player)
			if not model.Parent then
				return
			end
			ScoreService.award(player, CargoConfig.WrangleScore)
			toastAll(
				("%s %s wrangled the crocodile! +%d"):format(
					EMOJI_CROC,
					player.DisplayName,
					CargoConfig.WrangleScore
				)
			)
			model:Destroy()
		end)

		toastAll(EMOJI_CROC .. " THE CROCODILE IS LOOSE!")
	end

	local function breakCage(record: CargoRecord)
		if record.broken then
			return
		end
		record.broken = true
		local part = record.part
		local animals = part:GetAttribute("Animals")
		part.Transparency = 0.6
		part.CanCollide = false
		part:SetAttribute("CargoKg", CargoConfig.CageEmptyMass)
		if animals == "Chicken" then
			toastAll(EMOJI_CHICKEN .. " The chicken crate burst open!")
			for index = 1, CargoConfig.ChickenCount do
				spawnChicken(
					part.Position
						+ Vector3.new(RNG:NextNumber(-1.5, 1.5), 1, RNG:NextNumber(-1.5, 1.5))
				)
			end
		elseif animals == "Croc" then
			spawnCroc(part.Position)
		end
	end

	-- ============================== Cargo spawning ==============================

	local function addStrapPrompt(part: BasePart): ProximityPrompt
		local prompt = Instance.new("ProximityPrompt")
		prompt.Name = "StrapPrompt"
		prompt.ActionText = "Unstrap"
		prompt.ObjectText = "Cargo"
		prompt.HoldDuration = 0.4
		prompt.MaxActivationDistance = 7
		prompt.RequiresLineOfSight = false
		prompt.Parent = part
		return prompt
	end

	local function spawnBoxCargo(
		class: string,
		size: Vector3,
		color: Color3,
		material: Enum.Material,
		mass: number,
		localPosition: Vector3
	): CargoRecord
		local part = Instance.new("Part")
		part.Name = class
		part.Size = size
		part.Color = color
		part.Material = material
		part.CustomPhysicalProperties = PhysicalProperties.new(CargoConfig.CargoDensity, 0.4, 0.2)
		part.TopSurface = Enum.SurfaceType.Smooth
		part.BottomSurface = Enum.SurfaceType.Smooth
		part.CFrame = planeRoot.CFrame
			* CFrame.new(localPosition + Vector3.new(0, 0.7 + size.Y / 2, 0))
		part:SetAttribute("CargoKg", mass)
		part:SetAttribute("CargoClass", class)
		CollectionService:AddTag(part, Refs.Tags.CargoItem)
		part.Parent = cargoFolder

		local record: CargoRecord = {
			part = part,
			class = class,
			strapWeld = nil,
			strapPrompt = addStrapPrompt(part),
			broken = false,
		}
		table.insert(cargoItems, record)
		strapDown(record)

		record.strapPrompt.Triggered:Connect(function(player)
			if record.strapWeld then
				unstrap(record)
			elseif withinHold(part.Position) then
				strapDown(record)
			else
				bus.fire(player, "Toast", { text = "Get it back into the hold first!" })
			end
		end)

		-- Cages burst when they take a hard hit while loose.
		if
			part:GetAttribute("Animals") ~= nil
			or class == "ChickenCrate"
			or class == "CrocCage"
		then
			part.Touched:Connect(function(hit)
				if record.broken or record.strapWeld then
					return
				end
				local relativeSpeed = (part.AssemblyLinearVelocity - hit.AssemblyLinearVelocity).Magnitude
				if relativeSpeed >= CargoConfig.CageBreakImpactSpeed then
					breakCage(record)
				end
			end)
		end
		return record
	end

	local function wireLuggageThrow(tool: Tool)
		tool.Activated:Connect(function()
			local character = tool.Parent
			if not character or not character:IsA("Model") then
				return
			end
			local player = Players:GetPlayerFromCharacter(character)
			local root = character:FindFirstChild("HumanoidRootPart")
			local handle = tool:FindFirstChild("Handle")
			if
				not player
				or not root
				or not root:IsA("BasePart")
				or not handle
				or not handle:IsA("BasePart")
			then
				return
			end
			local look = root.CFrame.LookVector
			tool.Parent = workspace
			handle.CFrame = root.CFrame * CFrame.new(0, 1.5, -2.5)
			handle.AssemblyLinearVelocity = root.AssemblyLinearVelocity
				+ look * CargoConfig.ThrowSpeed
				+ Vector3.new(0, 8, 0)
		end)
	end

	local function spawnLuggage(localPosition: Vector3)
		local tool = Instance.new("Tool")
		tool.Name = "Luggage"
		tool.CanBeDropped = true
		tool:SetAttribute("Contents", "Luggage")
		tool:SetAttribute("CargoKg", CargoConfig.LuggageMass)

		local handle = Instance.new("Part")
		handle.Name = "Handle"
		handle.Size = Vector3.new(1.4, 1, 2)
		handle.Color = LUGGAGE_COLORS[RNG:NextInteger(1, #LUGGAGE_COLORS)]
		handle.Material = Enum.Material.SmoothPlastic
		handle.CustomPhysicalProperties = PhysicalProperties.new(CargoConfig.CargoDensity, 0.5, 0.2)
		handle.CFrame = planeRoot.CFrame * CFrame.new(localPosition + Vector3.new(0, 1.3, 0))
		handle.Parent = tool

		wireLuggageThrow(tool)
		tool.Parent = workspace
		table.insert(luggageTools, tool)
	end

	local function spawnLoadout()
		-- Slot grid along the racks: x alternates, z advances every two slots.
		local slotIndex = 0
		local function nextSlot(): Vector3
			local x = if slotIndex % 2 == 0 then -2.2 else 2.2
			local z = 25 + math.floor(slotIndex / 2) * 3.2
			slotIndex += 1
			return Vector3.new(x, 0, z)
		end

		for _ = 1, CargoConfig.CrateCount do
			spawnBoxCargo(
				"Crate",
				Vector3.new(2.8, 2.8, 2.8),
				Color3.fromRGB(160, 120, 70),
				Enum.Material.WoodPlanks,
				CargoConfig.CrateMass,
				nextSlot()
			)
		end
		for _ = 1, CargoConfig.PianoCount do
			spawnBoxCargo(
				"Piano",
				Vector3.new(4.5, 3, 2.2),
				Color3.fromRGB(20, 20, 24),
				Enum.Material.SmoothPlastic,
				CargoConfig.PianoMass,
				nextSlot()
			)
		end

		local chickenCage = spawnBoxCargo(
			"ChickenCrate",
			Vector3.new(2.4, 2.2, 2.4),
			Color3.fromRGB(180, 150, 90),
			Enum.Material.DiamondPlate,
			CargoConfig.ChickenCrateMass,
			nextSlot()
		)
		chickenCage.part:SetAttribute("Animals", "Chicken")

		local crocCage = spawnBoxCargo(
			"CrocCage",
			Vector3.new(3.6, 2.6, 2.6),
			Color3.fromRGB(110, 115, 120),
			Enum.Material.DiamondPlate,
			CargoConfig.CrocCageMass,
			nextSlot()
		)
		crocCage.part:SetAttribute("Animals", "Croc")

		for _ = 1, CargoConfig.LuggageCount do
			spawnLuggage(nextSlot())
		end
	end

	spawnLoadout()

	-- ============================== Critter tick ==============================

	task.spawn(function()
		while true do
			task.wait(CargoConfig.CritterTickSeconds)
			local now = os.clock()
			for index = #critters, 1, -1 do
				local critter = critters[index]
				if not critter.model.Parent then
					table.remove(critters, index)
					continue
				end
				if
					(critter.root.Position - planeRoot.Position).Magnitude
					> CargoConfig.DespawnDistanceFromPlane
				then
					critter.model:Destroy()
					table.remove(critters, index)
					continue
				end

				if critter.kind == "Chicken" then
					if now >= critter.nextActAt then
						critter.root:ApplyImpulse(
							Vector3.new(
								RNG:NextNumber(-1, 1) * CargoConfig.ChickenHopSideSpeed,
								CargoConfig.ChickenHopUpSpeed,
								RNG:NextNumber(-1, 1) * CargoConfig.ChickenHopSideSpeed
							) * critter.root.AssemblyMass
						)
						critter.nextActAt = now
							+ RNG:NextNumber(
								CargoConfig.ChickenHopMinSeconds,
								CargoConfig.ChickenHopMaxSeconds
							)
					end
				elseif critter.kind == "Croc" then
					local humanoid = critter.humanoid
					if not humanoid or humanoid.Health <= 0 then
						continue
					end
					local nearest: { humanoid: Humanoid, root: BasePart }? = nil
					local nearestDistance = CargoConfig.CrocAggroRange
					for _, target in collectBiteTargets() do
						local distance = (target.root.Position - critter.root.Position).Magnitude
						if distance < nearestDistance then
							nearest = target
							nearestDistance = distance
						end
					end
					if nearest then
						humanoid:MoveTo(nearest.root.Position)
						if
							nearestDistance <= CargoConfig.CrocBiteRange
							and now >= critter.nextActAt
						then
							critter.nextActAt = now + CargoConfig.CrocBiteCooldownSeconds
							local victim = nearest.humanoid
							victim:TakeDamage(CargoConfig.CrocBiteDamage)
							if victim.Health > 0 and victim.SeatPart == nil then
								victim.PlatformStand = true
								local away = (nearest.root.Position - critter.root.Position)
								local flat = Vector3.new(away.X, 0, away.Z)
								flat = if flat.Magnitude > 0.05 then flat.Unit else Vector3.xAxis
								nearest.root:ApplyImpulse(
									(flat * 18 + Vector3.new(0, 10, 0)) * nearest.root.AssemblyMass
								)
								task.delay(CargoConfig.CrocKnockdownSeconds, function()
									if victim.Health > 0 then
										victim.PlatformStand = false
									end
								end)
							end
						end
					end
				end
			end
		end
	end)

	-- ============================== Cargo tick ==============================

	task.spawn(function()
		while true do
			task.wait(CargoConfig.TickSeconds)
			local airspeed = readNumber(plane, "Airspeed", 0)
			local rampOpen = plane:GetAttribute("RampOpen") == true
			local aftDirection = -planeRoot.CFrame.LookVector
			local aboardMass = 0

			for index = #cargoItems, 1, -1 do
				local record = cargoItems[index]
				local part = record.part
				if not part.Parent then
					table.remove(cargoItems, index)
					continue
				end
				local distance = (part.Position - planeRoot.Position).Magnitude
				if distance > CargoConfig.DespawnDistanceFromPlane then
					part:Destroy()
					table.remove(cargoItems, index)
					continue
				end
				if withinPlane(part.Position) then
					aboardMass += readNumber(part, "CargoKg", 0)
					if
						rampOpen
						and not record.strapWeld
						and airspeed >= CargoConfig.RampWindMinAirspeed
					then
						part:ApplyImpulse(
							aftDirection
								* CargoConfig.AftPullSpeedPerSecond
								* CargoConfig.TickSeconds
								* part.AssemblyMass
						)
					end
				end
			end

			for index = #luggageTools, 1, -1 do
				local tool = luggageTools[index]
				if not tool.Parent then
					table.remove(luggageTools, index)
					continue
				end
				local handle = tool:FindFirstChild("Handle")
				if tool.Parent == workspace and handle and handle:IsA("BasePart") then
					local distance = (handle.Position - planeRoot.Position).Magnitude
					if distance > CargoConfig.DespawnDistanceFromPlane then
						tool:Destroy()
						table.remove(luggageTools, index)
						continue
					end
					if withinPlane(handle.Position) then
						aboardMass += readNumber(tool, "CargoKg", 0)
						if rampOpen and airspeed >= CargoConfig.RampWindMinAirspeed then
							handle:ApplyImpulse(
								aftDirection
									* CargoConfig.AftPullSpeedPerSecond
									* CargoConfig.TickSeconds
									* handle.AssemblyMass
							)
						end
					end
				else
					-- Carried by a player: it still weighs the plane down.
					aboardMass += readNumber(tool, "CargoKg", 0)
				end
			end

			plane:SetAttribute("CargoMass", math.round(aboardMass))
			local excess = math.max(0, aboardMass - CargoConfig.CargoCapacityMass)
			plane:SetAttribute(
				"ExcessCargoMass",
				math.round(excess * CargoConfig.ExcessWeightScale * 10) / 10
			)
		end
	end)
end

return CargoService
