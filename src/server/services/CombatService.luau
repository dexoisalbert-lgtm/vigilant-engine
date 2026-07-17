--!strict
-- CombatService — Phase 5 (docs/GAME_PLAN.md, system 5: Wing-Walking Combat).
-- Server side of the wing gatlings and the hostile fighter AI.
--
-- Gatling hitreg: the gunner's client streams {dir} over the "Combat" channel;
-- the server validates (seated on a VE_GatlingSeat, rate-limited, sane unit
-- vector), raycasts from the gun, and falls back to a small forgiveness cone
-- against fighter roots. Tracers are server-side neon parts everyone sees.
--
-- Fighters are kinematic (anchored, moved by PivotTo each Heartbeat): they
-- chase a slowly rotating orbit point beside the airliner, and inside attack
-- range they fire tracer-bolt bursts that hurt characters and can crack hull
-- or ignite engines via DamageService.attackPart. They spawn in waves while
-- the plane is at speed, break off when it slows, and die to gatling fire
-- with an explosion and a kill score for the last hitter.

local Debris = game:GetService("Debris")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local CombatConfig = require(Shared:WaitForChild("CombatConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any
local DamageService = require(script.Parent:WaitForChild("DamageService")) :: any

local CombatService = {}
CombatService.name = "CombatService"

local RNG = Random.new()

local EMOJI_ALERT = "\u{26A0}\u{FE0F}"
local EMOJI_BOOM = "\u{1F4A5}"

local COLOR_TRACER_FRIENDLY = Color3.fromRGB(255, 220, 90)
local COLOR_TRACER_HOSTILE = Color3.fromRGB(255, 80, 60)
local COLOR_FIGHTER = Color3.fromRGB(96, 44, 44)

type Fighter = {
	model: Model,
	root: BasePart,
	health: number,
	orbitPhase: number,
	direction: Vector3,
	nextBurstAt: number,
	lastAttacker: Player?,
	spawnedAt: number,
}

function CombatService.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local planeRoot = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart
	local combatBus = NetBus.server("Combat")
	local cabinBus = NetBus.server("Cabin")

	local fighters: { Fighter } = {}
	local fightersFolder = Instance.new("Folder")
	fightersFolder.Name = "VE_Fighters"
	fightersFolder.Parent = workspace

	local function toastAll(text: string)
		cabinBus.fireAll("Toast", { text = text })
	end

	local function readAirspeed(): number
		local value = plane:GetAttribute("Airspeed")
		return if typeof(value) == "number" then value else 0
	end

	local function tracer(from: Vector3, to: Vector3, color: Color3)
		local length = (to - from).Magnitude
		if length < 1 then
			return
		end
		local bolt = Instance.new("Part")
		bolt.Name = "Tracer"
		bolt.Material = Enum.Material.Neon
		bolt.Color = color
		bolt.Anchored = true
		bolt.CanCollide = false
		bolt.CanQuery = false
		bolt.Size = Vector3.new(0.15, 0.15, length)
		bolt.CFrame = CFrame.lookAt((from + to) / 2, to)
		bolt.Parent = workspace
		Debris:AddItem(bolt, 0.15)
	end

	local function killFighter(fighter: Fighter)
		local explosion = Instance.new("Explosion")
		explosion.Position = fighter.root.Position
		explosion.BlastRadius = 6
		explosion.BlastPressure = 0
		explosion.DestroyJointRadiusPercent = 0
		explosion.Parent = workspace
		if fighter.lastAttacker then
			ScoreService.award(fighter.lastAttacker, CombatConfig.FighterKillScore)
			toastAll(
				("%s %s shot down a fighter! +%d"):format(
					EMOJI_BOOM,
					fighter.lastAttacker.DisplayName,
					CombatConfig.FighterKillScore
				)
			)
		else
			toastAll(EMOJI_BOOM .. " Hostile fighter destroyed!")
		end
		fighter.model:Destroy()
	end

	-- ============================== Gatling hitreg ==============================

	local lastFireAt: { [Player]: number } = {}
	Players.PlayerRemoving:Connect(function(player)
		lastFireAt[player] = nil
	end)

	combatBus.on("Fire", function(player, data)
		local direction = data and data.dir
		if typeof(direction) ~= "Vector3" then
			return
		end
		local magnitude = direction.Magnitude
		if magnitude < 0.9 or magnitude > 1.1 then
			return
		end
		direction = direction.Unit

		local character = player.Character
		local humanoid = character and character:FindFirstChildOfClass("Humanoid")
		local seatPart = humanoid and humanoid.SeatPart
		if not seatPart or not seatPart:HasTag(Refs.Tags.GatlingSeat) then
			return
		end

		local now = os.clock()
		if now - (lastFireAt[player] or 0) < CombatConfig.GatlingFireCooldownSeconds then
			return
		end
		lastFireAt[player] = now

		local origin = seatPart.Position + Vector3.new(0, 1.8, 0) + direction * 4
		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		params.FilterDescendantsInstances = { plane, character :: Instance }
		local result = workspace:Raycast(origin, direction * CombatConfig.GatlingRange, params)

		local hitFighter: Fighter? = nil
		if result then
			for _, fighter in fighters do
				if result.Instance:IsDescendantOf(fighter.model) then
					hitFighter = fighter
					break
				end
			end
		end
		if not hitFighter then
			-- Forgiveness cone against fighter roots.
			local bestDot = CombatConfig.GatlingForgivenessCos
			for _, fighter in fighters do
				local offset = fighter.root.Position - origin
				local distance = offset.Magnitude
				if distance <= CombatConfig.GatlingRange and distance > 1 then
					local dot = offset.Unit:Dot(direction)
					if dot >= bestDot then
						bestDot = dot
						hitFighter = fighter
					end
				end
			end
		end

		local endpoint = if hitFighter
			then hitFighter.root.Position
			elseif result then result.Position
			else origin + direction * CombatConfig.GatlingRange
		tracer(origin, endpoint, COLOR_TRACER_FRIENDLY)

		if hitFighter and hitFighter.model.Parent then
			hitFighter.health -= CombatConfig.GatlingDamage
			hitFighter.lastAttacker = player
			if hitFighter.health <= 0 then
				killFighter(hitFighter)
			end
		end
	end)

	-- ============================== Fighters ==============================

	local function spawnFighter()
		local model = Instance.new("Model")
		model.Name = "HostileFighter"

		local body = Instance.new("Part")
		body.Name = "Body"
		body.Size = Vector3.new(2, 2, 7)
		body.Color = COLOR_FIGHTER
		body.Material = Enum.Material.Metal
		body.Anchored = true
		body.CanCollide = false
		body.Parent = model

		local wings = Instance.new("Part")
		wings.Name = "Wings"
		wings.Size = Vector3.new(9, 0.4, 2.4)
		wings.Color = COLOR_FIGHTER
		wings.Material = Enum.Material.Metal
		wings.Anchored = true
		wings.CanCollide = false
		wings.Parent = model

		local fin = Instance.new("Part")
		fin.Name = "Fin"
		fin.Size = Vector3.new(0.4, 1.6, 1.6)
		fin.Color = Color3.fromRGB(40, 40, 44)
		fin.Material = Enum.Material.Metal
		fin.Anchored = true
		fin.CanCollide = false
		fin.Parent = model

		model.PrimaryPart = body
		local weldWings = Instance.new("WeldConstraint")
		weldWings.Part0 = body
		weldWings.Part1 = wings
		weldWings.Parent = wings
		local weldFin = Instance.new("WeldConstraint")
		weldFin.Part0 = body
		weldFin.Part1 = fin
		weldFin.Parent = fin

		local angle = RNG:NextNumber(0, 2 * math.pi)
		local spawnPos = planeRoot.Position
			+ Vector3.new(
				math.cos(angle) * CombatConfig.FighterSpawnDistance,
				RNG:NextNumber(-40, 80),
				math.sin(angle) * CombatConfig.FighterSpawnDistance
			)
		body.CFrame = CFrame.new(spawnPos)
		wings.CFrame = body.CFrame
		fin.CFrame = body.CFrame * CFrame.new(0, 1.6, 3)
		model:PivotTo(CFrame.lookAt(spawnPos, planeRoot.Position))
		model.Parent = fightersFolder

		local fighter: Fighter = {
			model = model,
			root = body,
			health = CombatConfig.FighterHealth,
			orbitPhase = RNG:NextNumber(0, 2 * math.pi),
			direction = (planeRoot.Position - spawnPos).Unit,
			nextBurstAt = os.clock() + 3,
			lastAttacker = nil,
			spawnedAt = os.clock(),
		}
		table.insert(fighters, fighter)
	end

	local function fireBolt(fighter: Fighter)
		if not fighter.model.Parent then
			return
		end
		local repairables = Refs.all(Refs.Tags.Repairable)
		local target: Vector3
		if #repairables > 0 then
			local part = repairables[RNG:NextInteger(1, #repairables)] :: BasePart
			target = part.Position
		else
			target = planeRoot.Position
		end
		target += Vector3.new(RNG:NextNumber(-5, 5), RNG:NextNumber(-3, 3), RNG:NextNumber(-5, 5))

		local origin = fighter.root.Position
		local direction = (target - origin)
		if direction.Magnitude < 1 then
			return
		end
		direction = direction.Unit

		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		params.FilterDescendantsInstances = { fightersFolder }
		local result = workspace:Raycast(origin, direction * CombatConfig.FighterBoltRange, params)
		local endpoint = if result
			then result.Position
			else origin + direction * CombatConfig.FighterBoltRange
		tracer(origin, endpoint, COLOR_TRACER_HOSTILE)

		if result then
			local characterModel = result.Instance:FindFirstAncestorOfClass("Model")
			local humanoid = characterModel and characterModel:FindFirstChildOfClass("Humanoid")
			if humanoid and humanoid.Health > 0 then
				humanoid:TakeDamage(CombatConfig.FighterBoltPlayerDamage)
			elseif
				result.Instance:IsDescendantOf(plane)
				and RNG:NextNumber() < CombatConfig.FighterBoltCrackChance
			then
				DamageService.attackPart(result.Instance)
			end
		end
	end

	local function fireBurst(fighter: Fighter)
		task.spawn(function()
			for _ = 1, CombatConfig.FighterBoltsPerBurst do
				fireBolt(fighter)
				task.wait(0.12)
			end
		end)
	end

	RunService.Heartbeat:Connect(function(dt: number)
		local now = os.clock()
		local planePos = planeRoot.Position
		for index = #fighters, 1, -1 do
			local fighter = fighters[index]
			if not fighter.model.Parent then
				table.remove(fighters, index)
				continue
			end
			if now - fighter.spawnedAt > CombatConfig.FighterLifetimeSeconds then
				fighter.model:Destroy()
				table.remove(fighters, index)
				continue
			end

			fighter.orbitPhase += CombatConfig.FighterOrbitAngularSpeed * dt
			local orbitTarget = planePos
				+ Vector3.new(
					math.cos(fighter.orbitPhase) * CombatConfig.FighterOrbitRadius,
					CombatConfig.FighterOrbitHeight,
					math.sin(fighter.orbitPhase) * CombatConfig.FighterOrbitRadius
				)
			local wanted = orbitTarget - fighter.root.Position
			if wanted.Magnitude > 1 then
				fighter.direction = fighter.direction:Lerp(
					wanted.Unit,
					math.clamp(CombatConfig.FighterTurnResponsiveness * dt, 0, 1)
				).Unit
			end
			local newPos = fighter.root.Position
				+ fighter.direction * CombatConfig.FighterSpeed * dt
			fighter.model:PivotTo(CFrame.lookAt(newPos, newPos + fighter.direction))

			if
				(planePos - newPos).Magnitude <= CombatConfig.FighterAttackRange
				and now >= fighter.nextBurstAt
			then
				fighter.nextBurstAt = now
					+ RNG:NextNumber(
						CombatConfig.FighterBurstMinSeconds,
						CombatConfig.FighterBurstMaxSeconds
					)
				fireBurst(fighter)
			end
		end
	end)

	-- Wave spawner + disengage when the plane slows down.
	task.spawn(function()
		while true do
			task.wait(
				RNG:NextNumber(
					CombatConfig.FighterSpawnMinSeconds,
					CombatConfig.FighterSpawnMaxSeconds
				)
			)
			if readAirspeed() < CombatConfig.FighterMinPlaneAirspeed then
				continue
			end
			local room = CombatConfig.FighterMaxActive - #fighters
			if room <= 0 then
				continue
			end
			local count = math.min(RNG:NextInteger(1, 2), room)
			for _ = 1, count do
				spawnFighter()
			end
			toastAll(EMOJI_ALERT .. " Hostile fighters inbound! Man the wing guns!")
		end
	end)

	task.spawn(function()
		while true do
			task.wait(5)
			if #fighters > 0 and readAirspeed() < CombatConfig.FighterDisengageAirspeed then
				for index = #fighters, 1, -1 do
					fighters[index].model:Destroy()
					table.remove(fighters, index)
				end
				toastAll("The fighters broke off pursuit.")
			end
		end
	end)
end

return CombatService
