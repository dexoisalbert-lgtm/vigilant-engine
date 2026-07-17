--!strict
-- MissionService — Phase 6 (docs/GAME_PLAN.md, rounds + system 6: Smuggling).
-- Runs the round loop: intermission (airframe repaired, fuel topped up) ->
-- mission -> win/lose -> next. Three mission types rotate:
--   Delivery — fly to a beacon airfield and land on the pad.
--   Smuggle  — contraband appears behind the smuggler panel; hurl packages
--              onto ground drop zones (trucks waiting), dodge the air
--              marshal patrolling the cabin, then land back home.
--   Defend   — survive fighter waves (via CombatService), then land.
--
-- Mission state is published as attributes on ReplicatedStorage.VE_MissionState
-- (MissionId, MissionName, Phase, SecondsRemaining, Objective, Contraband*),
-- which the client MissionController renders. Failure comes from the timer
-- running out, a crash-hard touchdown, or (Smuggle) losing too many packages.
--
-- The air marshal arrests anyone caught holding contraband OUT (equipped) --
-- packages riding in a backpack stay unnoticed. Arrest = confiscation plus a
-- forced sit in a cabin seat for a few seconds.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local MissionConfig = require(Shared:WaitForChild("MissionConfig"))
local FlightConfig = require(Shared:WaitForChild("FlightConfig"))
local CombatConfig = require(Shared:WaitForChild("CombatConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any
local StatsService = require(script.Parent:WaitForChild("StatsService")) :: any
local DamageService = require(script.Parent:WaitForChild("DamageService")) :: any
local CombatService = require(script.Parent:WaitForChild("CombatService")) :: any
local PassengerService = require(script.Parent:WaitForChild("PassengerService")) :: any

local MissionService = {}
MissionService.name = "MissionService"

local RNG = Random.new()
local TICK = 0.5

local EMOJI_TARGET = "\u{1F3AF}"
local EMOJI_WIN = "\u{1F3C6}"
local EMOJI_LOSE = "\u{1F480}"
local EMOJI_PACKAGE = "\u{1F4E6}"
local EMOJI_SIREN = "\u{1F6A8}"

function MissionService.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local planeRoot = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart
	local bus = NetBus.server("Cabin")

	local state = Instance.new("Folder")
	state.Name = "VE_MissionState"
	state:SetAttribute("MissionId", "")
	state:SetAttribute("MissionName", "Boarding")
	state:SetAttribute("Phase", "Boarding")
	state:SetAttribute("SecondsRemaining", 0)
	state:SetAttribute("Objective", "")
	state:SetAttribute("ContrabandDelivered", 0)
	state:SetAttribute("ContrabandTotal", 0)
	state.Parent = ReplicatedStorage

	local propsFolder = Instance.new("Folder")
	propsFolder.Name = "VE_MissionProps"
	propsFolder.Parent = workspace

	local function toastAll(text: string)
		bus.fireAll("Toast", { text = text })
	end

	local function readNumber(instance: Instance, name: string, fallback: number): number
		local value = instance:GetAttribute(name)
		return if typeof(value) == "number" then value else fallback
	end

	local groundParams = RaycastParams.new()
	groundParams.FilterType = Enum.RaycastFilterType.Exclude
	groundParams.FilterDescendantsInstances = { plane }

	local function isGrounded(): boolean
		return workspace:Raycast(planeRoot.Position, Vector3.new(0, -4, 0), groundParams) ~= nil
	end

	-- ==================== Smuggler panel (always available) ====================

	task.spawn(function()
		local panel = Refs.waitFor(Refs.Tags.SmugglerHold) :: BasePart
		local prompt = panel:FindFirstChildOfClass("ProximityPrompt")
		if not prompt then
			return
		end
		prompt.Triggered:Connect(function()
			local open = panel:GetAttribute("PanelOpen") ~= true
			panel:SetAttribute("PanelOpen", open)
			panel.Transparency = if open then 0.7 else 0
			panel.CanCollide = not open
			prompt.ActionText = if open then "Close Panel" else "Open Panel"
		end)
	end)

	-- ==================== Mission props ====================

	local function randomGroundSpot(minDistance: number, maxDistance: number): Vector3
		local angle = RNG:NextNumber(0, 2 * math.pi)
		local distance = RNG:NextNumber(minDistance, maxDistance)
		return Vector3.new(math.cos(angle) * distance, 0, math.sin(angle) * distance)
	end

	local function buildBeacon(position: Vector3, color: Color3, height: number): Part
		local beacon = Instance.new("Part")
		beacon.Name = "Beacon"
		beacon.Size = Vector3.new(2, height, 2)
		beacon.CFrame = CFrame.new(position + Vector3.new(0, height / 2, 0))
		beacon.Color = color
		beacon.Material = Enum.Material.Neon
		beacon.Transparency = 0.25
		beacon.Anchored = true
		beacon.CanCollide = false
		beacon.Parent = propsFolder
		return beacon
	end

	local function buildDeliveryPad(position: Vector3): Part
		local pad = Instance.new("Part")
		pad.Name = "DeliveryPad"
		pad.Size = Vector3.new(80, 1, 160)
		pad.CFrame = CFrame.new(position + Vector3.new(0, 0.5, 0))
		pad.Color = Color3.fromRGB(90, 90, 96)
		pad.Material = Enum.Material.Concrete
		pad.Anchored = true
		pad.Parent = propsFolder
		buildBeacon(position, Color3.fromRGB(255, 170, 60), 140)
		return pad
	end

	local function buildDropZone(position: Vector3): { pad: Part, light: Part }
		local pad = Instance.new("Part")
		pad.Name = "DropZone"
		pad.Shape = Enum.PartType.Cylinder
		pad.Size =
			Vector3.new(0.4, MissionConfig.DropZoneRadius * 2, MissionConfig.DropZoneRadius * 2)
		pad.CFrame = CFrame.new(position + Vector3.new(0, 0.3, 0))
			* CFrame.Angles(0, 0, math.rad(90))
		pad.Color = Color3.fromRGB(90, 200, 90)
		pad.Material = Enum.Material.Neon
		pad.Transparency = 0.4
		pad.Anchored = true
		pad:AddTag(Refs.Tags.DropZone)
		pad.Parent = propsFolder

		local truck = Instance.new("Part")
		truck.Name = "AccompliceTruck"
		truck.Size = Vector3.new(6, 3, 3)
		truck.CFrame = CFrame.new(position + Vector3.new(MissionConfig.DropZoneRadius + 5, 1.5, 0))
		truck.Color = Color3.fromRGB(40, 40, 46)
		truck.Anchored = true
		truck.Parent = propsFolder

		local light = Instance.new("Part")
		light.Name = "TruckLight"
		light.Size = Vector3.new(0.8, 0.8, 0.8)
		light.CFrame = truck.CFrame * CFrame.new(0, 1.9, 0)
		light.Color = Color3.fromRGB(120, 40, 40)
		light.Material = Enum.Material.Neon
		light.Anchored = true
		light.Parent = propsFolder

		buildBeacon(position, Color3.fromRGB(90, 200, 90), 100)
		return { pad = pad, light = light }
	end

	local function flashLight(light: Part)
		task.spawn(function()
			for _ = 1, 6 do
				if not light.Parent then
					return
				end
				light.Color = Color3.fromRGB(255, 70, 70)
				task.wait(0.2)
				light.Color = Color3.fromRGB(120, 40, 40)
				task.wait(0.2)
			end
		end)
	end

	-- ==================== Contraband packages ====================

	local function makePackage(spawnCf: CFrame): Tool
		local tool = Instance.new("Tool")
		tool.Name = "Package"
		tool.CanBeDropped = true
		tool:SetAttribute("Contents", "Contraband")

		local handle = Instance.new("Part")
		handle.Name = "Handle"
		handle.Size = Vector3.new(1.2, 1, 1.2)
		handle.Color = Color3.fromRGB(150, 110, 70)
		handle.Material = Enum.Material.SmoothPlastic
		handle.CustomPhysicalProperties = PhysicalProperties.new(0.3, 0.5, 0.2)
		handle.CFrame = spawnCf
		handle.Parent = tool

		tool.Activated:Connect(function()
			local character = tool.Parent
			if not character or not character:IsA("Model") then
				return
			end
			local root = character:FindFirstChild("HumanoidRootPart")
			if not root or not root:IsA("BasePart") then
				return
			end
			tool.Parent = workspace
			handle.CFrame = root.CFrame * CFrame.new(0, 1.5, -2.5)
			handle.AssemblyLinearVelocity = root.AssemblyLinearVelocity
				+ root.CFrame.LookVector * 40
				+ Vector3.new(0, 8, 0)
		end)
		tool.Parent = workspace
		return tool
	end

	-- ==================== Air marshal ====================

	local function spawnMarshal(): Model
		local model = PassengerService.makeNpcRig() :: Model
		model.Name = "AirMarshal"
		local humanoid = model:FindFirstChildOfClass("Humanoid") :: Humanoid
		humanoid.DisplayName = "Air Marshal"
		humanoid.WalkSpeed = MissionConfig.MarshalWalkSpeed
		for _, descendant in model:GetDescendants() do
			if descendant:IsA("BasePart") and descendant.Name ~= "Head" then
				descendant.Color = Color3.fromRGB(35, 40, 60)
			end
		end
		model.Parent = propsFolder
		model:PivotTo(planeRoot.CFrame * CFrame.new(0, 3, 16))
		local root = model:FindFirstChild("HumanoidRootPart")
		if root and root:IsA("BasePart") then
			pcall(function()
				root:SetNetworkOwner(nil)
			end)
		end
		toastAll(EMOJI_SIREN .. " An air marshal boarded the plane. Keep the goods hidden!")
		return model
	end

	local cuffedUntil: { [Player]: number } = {}
	Players.PlayerRemoving:Connect(function(player)
		cuffedUntil[player] = nil
	end)

	local function arrest(player: Player, tool: Tool)
		tool:Destroy()
		cuffedUntil[player] = os.clock() + MissionConfig.MarshalCuffSeconds
		toastAll(
			("%s The marshal busted %s! Contraband confiscated."):format(
				EMOJI_SIREN,
				player.DisplayName
			)
		)
		local character = player.Character
		local humanoid = character and character:FindFirstChildOfClass("Humanoid")
		if not character or not humanoid then
			return
		end
		for _, instance in Refs.all(Refs.Tags.CabinSeat) do
			local seat = instance :: Seat
			if not seat.Occupant then
				character:PivotTo(seat.CFrame * CFrame.new(0, 3, 0))
				seat:Sit(humanoid)
				break
			end
		end
	end

	local function marshalTick(marshal: Model, moveState: { moveAt: number })
		local humanoid = marshal:FindFirstChildOfClass("Humanoid")
		local root = marshal:FindFirstChild("HumanoidRootPart")
		if not humanoid or humanoid.Health <= 0 or not root or not root:IsA("BasePart") then
			return
		end
		local now = os.clock()
		if now >= moveState.moveAt then
			local wander = planeRoot.CFrame:PointToWorldSpace(
				Vector3.new(RNG:NextNumber(-3, 3), 1.5, RNG:NextNumber(-12, 20))
			)
			humanoid:MoveTo(wander)
			moveState.moveAt = now + 3
		end
		for _, player in Players:GetPlayers() do
			local character = player.Character
			local playerRoot = character and character:FindFirstChild("HumanoidRootPart")
			local tool = character and character:FindFirstChildOfClass("Tool")
			if
				character
				and playerRoot
				and playerRoot:IsA("BasePart")
				and tool
				and tool:GetAttribute("Contents") == "Contraband"
				and (playerRoot.Position - root.Position).Magnitude
					<= MissionConfig.MarshalDetectRadius
			then
				arrest(player, tool)
			end
		end
		-- Keep cuffed players in their seats.
		for player, untilTime in cuffedUntil do
			if os.clock() >= untilTime then
				cuffedUntil[player] = nil
			else
				local character = player.Character
				local humanoidCuffed = character and character:FindFirstChildOfClass("Humanoid")
				if character and humanoidCuffed and humanoidCuffed.SeatPart == nil then
					for _, instance in Refs.all(Refs.Tags.CabinSeat) do
						local seat = instance :: Seat
						if not seat.Occupant then
							character:PivotTo(seat.CFrame * CFrame.new(0, 3, 0))
							seat:Sit(humanoidCuffed)
							break
						end
					end
				end
			end
		end
	end

	-- ==================== Shared mission runner ====================

	-- Polls checkFn every TICK until it returns "win" or "lose:<reason>", the
	-- timer expires, or the plane crash-lands. Returns (won, reason).
	local function runMission(durationSeconds: number, checkFn: () -> string?): (boolean, string)
		local deadline = os.clock() + durationSeconds
		local wasAirborne = false
		local previousVs = 0
		while true do
			task.wait(TICK)
			local remaining = math.max(0, math.floor(deadline - os.clock()))
			state:SetAttribute("SecondsRemaining", remaining)

			local airspeed = readNumber(plane, "Airspeed", 0)
			local verticalSpeed = readNumber(plane, "VerticalSpeed", 0)
			local grounded = isGrounded()

			if
				state:GetAttribute("Phase") == "Takeoff"
				and airspeed > MissionConfig.TakeoffAirspeed
			then
				state:SetAttribute("Phase", "Cruise")
			end

			if wasAirborne and grounded and previousVs < -MissionConfig.CrashVerticalSpeed then
				return false, "The plane slammed into the ground."
			end
			wasAirborne = not grounded
			previousVs = verticalSpeed

			local outcome = checkFn()
			if outcome == "win" then
				return true, ""
			elseif outcome and outcome:sub(1, 5) == "lose:" then
				return false, outcome:sub(6)
			end

			if remaining <= 0 then
				return false, "Ran out of time."
			end
		end
	end

	-- Landing dwell helper: counts consecutive ticks parked in a spot.
	local function makeLandingCheck(isAtTarget: () -> boolean): () -> boolean
		local dwell = 0
		return function()
			local airspeed = readNumber(plane, "Airspeed", 0)
			if isGrounded() and airspeed < MissionConfig.LandingAirspeed and isAtTarget() then
				dwell += TICK
			else
				dwell = 0
			end
			return dwell >= MissionConfig.LandingDwellSeconds
		end
	end

	-- ==================== Mission runners ====================

	local function runDelivery(def: any): (boolean, string)
		local spot =
			randomGroundSpot(MissionConfig.DeliveryDistanceMin, MissionConfig.DeliveryDistanceMax)
		local pad = buildDeliveryPad(spot)
		local landed = makeLandingCheck(function()
			local offset = planeRoot.Position - pad.Position
			return Vector3.new(offset.X, 0, offset.Z).Magnitude <= MissionConfig.DeliveryWinRadius
		end)
		return runMission(def.durationSeconds, function()
			return if landed() then "win" else nil
		end)
	end

	local function runSmuggle(def: any): (boolean, string)
		local zones = {}
		for _ = 1, def.zoneCount do
			table.insert(
				zones,
				buildDropZone(
					randomGroundSpot(MissionConfig.ZoneDistanceMin, MissionConfig.ZoneDistanceMax)
				)
			)
		end

		local panel = Refs.find(Refs.Tags.SmugglerHold) :: BasePart?
		local packages: { [Tool]: { landedAt: number? } } = {}
		for index = 1, def.packageCount do
			local spawnCf = if panel
				then panel.CFrame * CFrame.new(((index - 1) % 2) * 1.2 - 0.6, 0, -1.2)
				else planeRoot.CFrame * CFrame.new(-3, 2, 24)
			packages[makePackage(spawnCf)] = {}
		end
		local required = math.max(1, def.packageCount - 1)
		local delivered = 0
		state:SetAttribute("ContrabandTotal", def.packageCount)
		state:SetAttribute("ContrabandDelivered", 0)

		local marshal = spawnMarshal()
		local marshalMove = { moveAt = 0 }

		local landedHome = makeLandingCheck(function()
			local position = planeRoot.Position
			return Vector3.new(position.X, 0, position.Z).Magnitude <= MissionConfig.HomeRadius
		end)

		local won, reason = runMission(def.durationSeconds, function()
			marshalTick(marshal, marshalMove)

			local alive = 0
			for tool, info in packages do
				if not tool.Parent then
					packages[tool] = nil
					continue
				end
				if tool:GetAttribute("Contents") ~= "Contraband" then
					continue
				end
				alive += 1
				local handle = tool:FindFirstChild("Handle")
				if tool.Parent == workspace and handle and handle:IsA("BasePart") then
					local position = handle.Position
					local inZone = false
					for _, zone in zones do
						local offset = position - zone.pad.Position
						if
							Vector3.new(offset.X, 0, offset.Z).Magnitude
								<= MissionConfig.DropZoneRadius
							and position.Y <= zone.pad.Position.Y + 8
						then
							inZone = true
							flashLight(zone.light)
							break
						end
					end
					if inZone then
						packages[tool] = nil
						tool:Destroy()
						delivered += 1
						state:SetAttribute("ContrabandDelivered", delivered)
						toastAll(
							("%s Package delivered! (%d/%d)"):format(
								EMOJI_PACKAGE,
								delivered,
								required
							)
						)
					elseif position.Y <= 6 and handle.AssemblyLinearVelocity.Magnitude < 3 then
						-- Landed outside every zone; lost after a grace period.
						info.landedAt = info.landedAt or os.clock()
						if
							os.clock() - (info.landedAt :: number)
							> MissionConfig.PackageLostSeconds
						then
							packages[tool] = nil
							tool:Destroy()
							toastAll(EMOJI_PACKAGE .. " A package was lost in the wilderness...")
						end
					else
						info.landedAt = nil
					end
				end
			end

			if delivered >= required then
				state:SetAttribute("Objective", "Deliveries done — land back at the home runway!")
				return if landedHome() then "win" else nil
			end
			if delivered + alive < required then
				return "lose:Too many packages were lost or confiscated."
			end
			return nil
		end)

		for tool in packages do
			tool:Destroy()
		end
		marshal:Destroy()
		return won, reason
	end

	local function runDefend(def: any): (boolean, string)
		local wavesLeft = def.waves
		local nextWaveAt = os.clock() + 20
		local allCleared = false
		local landed = makeLandingCheck(function()
			return true
		end)
		return runMission(def.durationSeconds, function()
			if not allCleared then
				if
					wavesLeft > 0
					and CombatService.activeFighters() == 0
					and os.clock() >= nextWaveAt
					and readNumber(plane, "Airspeed", 0) > CombatConfig.FighterMinPlaneAirspeed
				then
					wavesLeft -= 1
					CombatService.spawnWave(def.waveSize)
					nextWaveAt = os.clock() + 15
					toastAll(
						("%s Wave incoming! %d more after this."):format(EMOJI_TARGET, wavesLeft)
					)
				elseif wavesLeft == 0 and CombatService.activeFighters() == 0 then
					allCleared = true
					state:SetAttribute("Objective", "All waves cleared — land anywhere!")
					toastAll(EMOJI_WIN .. " Skies clear! Bring her down.")
				end
				return nil
			end
			return if landed() then "win" else nil
		end)
	end

	local runners: { [string]: (any) -> (boolean, string) } = {
		Delivery = runDelivery,
		Smuggle = runSmuggle,
		Defend = runDefend,
	}

	-- ==================== Round loop ====================

	task.spawn(function()
		local rotationIndex = 0
		while true do
			-- Intermission: repair, refuel, board.
			state:SetAttribute("MissionId", "")
			state:SetAttribute("MissionName", "Boarding")
			state:SetAttribute("Phase", "Boarding")
			state:SetAttribute("Objective", "Repairing and refueling — get aboard!")
			state:SetAttribute("ContrabandTotal", 0)
			state:SetAttribute("ContrabandDelivered", 0)
			DamageService.resetAll()
			plane:SetAttribute("FuelKg", FlightConfig.FuelCapacityKg)
			for remaining = MissionConfig.IntermissionSeconds, 1, -1 do
				state:SetAttribute("SecondsRemaining", remaining)
				task.wait(1)
			end

			rotationIndex = rotationIndex % #MissionConfig.Rotation + 1
			local kindId = MissionConfig.Rotation[rotationIndex]
			local def = (MissionConfig.Definitions :: any)[kindId]

			state:SetAttribute("MissionId", kindId)
			state:SetAttribute("MissionName", def.name)
			state:SetAttribute("Phase", "Takeoff")
			state:SetAttribute("Objective", def.objective)
			toastAll(("%s Mission: %s — %s"):format(EMOJI_TARGET, def.name, def.objective))

			local won, reason = runners[kindId](def)

			state:SetAttribute("Phase", "Ended")
			state:SetAttribute("SecondsRemaining", 0)
			if won then
				for _, player in Players:GetPlayers() do
					ScoreService.award(player, MissionConfig.MissionWinScore)
					StatsService.addStat(player, "Wins", 1)
				end
				state:SetAttribute("Objective", "Mission complete!")
				toastAll(
					("%s Mission complete! +%d for the whole crew."):format(
						EMOJI_WIN,
						MissionConfig.MissionWinScore
					)
				)
			else
				state:SetAttribute("Objective", "Mission failed: " .. reason)
				toastAll(EMOJI_LOSE .. " Mission failed. " .. reason)
			end

			propsFolder:ClearAllChildren()
			task.wait(MissionConfig.EndedHoldSeconds)
		end
	end)
end

return MissionService
