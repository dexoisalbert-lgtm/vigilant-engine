--!strict
-- DamageService — Phase 3 (docs/GAME_PLAN.md, system 3: Emergency Maintenance).
-- Owns the Damage/State/RepairKind attributes on every VE_Repairable part and
-- is the only mutator; everything else just reads.
--
-- Lifecycle: hazards (random while airborne; missions take over in Phase 6)
-- crack hull panels, ignite engines, ice wings, and throw lightning. Cracked
-- hull left unrepaired tears off entirely (Detached); burning engines drain
-- EngineHealth (FlightController reads it for thrust) and detach at zero.
-- Cracked/Detached hull depressurizes: loose characters near the breach get
-- pulled toward it every escalation tick.
--
-- Detachment is virtual: a cosmetic clone is flung off and the original part
-- goes invisible/non-collidable, so restoring it is just a visual flip — the
-- weld to the plane root is never touched.
--
-- Repairs are ProximityPrompts the service adds to damaged parts, gated on
-- the player's equipped tool having a matching RepairKind attribute.

local Debris = game:GetService("Debris")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local DamageConfig = require(Shared:WaitForChild("DamageConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any

local DamageService = {}
DamageService.name = "DamageService"

local RNG = Random.new()

local EMOJI_BREACH = "\u{1F4A8}"
local EMOJI_FIRE = "\u{1F525}"
local EMOJI_ICE = "\u{1F9CA}"
local EMOJI_BOLT = "\u{26A1}"
local EMOJI_BANG = "\u{1F4A5}"
local EMOJI_WRENCH = "\u{1F527}"

local TOOL_FOR_KIND: { [string]: string } = {
	Weld = "Blowtorch",
	Hammer = "Hammer",
	Extinguisher = "Fire Extinguisher",
	IceAxe = "Ice Axe",
}

type PartRecord = {
	part: BasePart,
	zone: string, -- "Hull" | "Wing" | "Engine"
	originalColor: Color3,
	originalMaterial: Enum.Material,
	originalTransparency: number,
	crackedAt: number?,
	prompt: ProximityPrompt?,
	promptLabel: string,
	fire: Fire?,
	smoke: Smoke?,
}

local function readNumber(instance: Instance, name: string, fallback: number): number
	local value = instance:GetAttribute(name)
	return if typeof(value) == "number" then value else fallback
end

function DamageService.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local planeRoot = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart
	local bus = NetBus.server("Cabin")

	local records: { [BasePart]: PartRecord } = {}

	local function toastAll(text: string)
		bus.fireAll("Toast", { text = text })
	end

	local function publishHullIntegrity()
		local ok, total = 0, 0
		for _, record in records do
			if record.zone == "Hull" then
				total += 1
				if record.part:GetAttribute("State") == "Ok" then
					ok += 1
				end
			end
		end
		plane:SetAttribute("HullIntegrity", if total > 0 then math.round(100 * ok / total) else 100)
	end

	local function publishIceMass()
		local mass = 0
		for _, record in records do
			if record.part:GetAttribute("State") == "Iced" then
				local fraction = readNumber(record.part, "Damage", 0) / DamageConfig.IceDamage
				mass += fraction * DamageConfig.IceMassPerIcedPart
			end
		end
		plane:SetAttribute("IceMass", math.round(mass))
	end

	local function clearEffects(record: PartRecord)
		if record.fire then
			record.fire:Destroy()
			record.fire = nil
		end
		if record.smoke then
			record.smoke:Destroy()
			record.smoke = nil
		end
	end

	local function restore(record: PartRecord)
		local part = record.part
		part:SetAttribute("State", "Ok")
		part:SetAttribute("Damage", 0)
		part:SetAttribute("RepairKind", nil)
		part.Color = record.originalColor
		part.Material = record.originalMaterial
		part.Transparency = record.originalTransparency
		part.CanCollide = true
		record.crackedAt = nil
		clearEffects(record)
		if record.prompt then
			record.prompt:Destroy()
			record.prompt = nil
		end
		if record.zone == "Engine" then
			local health = readNumber(part, "EngineHealth", 0)
			part:SetAttribute("EngineHealth", math.max(health, DamageConfig.RestoredEngineHealth))
		end
		publishHullIntegrity()
		publishIceMass()
	end

	local function ensurePrompt(record: PartRecord, actionText: string, label: string)
		record.promptLabel = label
		local part = record.part
		local damage = readNumber(part, "Damage", 0)
		if record.prompt then
			record.prompt.ActionText = actionText
			record.prompt.ObjectText = ("%s %d%%"):format(label, damage)
			return
		end
		local prompt = Instance.new("ProximityPrompt")
		prompt.Name = "RepairPrompt"
		prompt.ActionText = actionText
		prompt.ObjectText = ("%s %d%%"):format(label, damage)
		prompt.HoldDuration = DamageConfig.RepairPromptHoldSeconds
		prompt.MaxActivationDistance = 10
		prompt.RequiresLineOfSight = false
		prompt.Parent = part
		record.prompt = prompt

		prompt.Triggered:Connect(function(player)
			local state = part:GetAttribute("State")
			if state == "Ok" then
				return
			end
			local needed = part:GetAttribute("RepairKind")
			if typeof(needed) ~= "string" then
				return
			end
			local character = player.Character
			local tool = character and character:FindFirstChildOfClass("Tool")
			if not tool or tool:GetAttribute("RepairKind") ~= needed then
				bus.fire(player, "Toast", {
					text = ("You need the %s!"):format(TOOL_FOR_KIND[needed] or "right tool"),
				})
				return
			end
			local power = DamageConfig.RepairPower[needed] or 25
			local remaining = math.max(0, readNumber(part, "Damage", 0) - power)
			part:SetAttribute("Damage", remaining)
			if state == "Iced" then
				publishIceMass()
			end
			if remaining <= 0 then
				restore(record)
				ScoreService.award(player, DamageConfig.RepairScore)
				bus.fire(player, "Toast", {
					text = ("%s +%d  Repaired %s"):format(
						EMOJI_WRENCH,
						DamageConfig.RepairScore,
						part.Name
					),
				})
			else
				prompt.ObjectText = ("%s %d%%"):format(record.promptLabel, remaining)
			end
		end)
	end

	local function crackPart(record: PartRecord)
		local part = record.part
		if part:GetAttribute("State") ~= "Ok" then
			return
		end
		part:SetAttribute("State", "Cracked")
		part:SetAttribute("Damage", DamageConfig.CrackDamage)
		part:SetAttribute("RepairKind", "Weld")
		record.crackedAt = os.clock()
		part.Material = Enum.Material.CorrodedMetal
		part.Color = record.originalColor:Lerp(Color3.new(0, 0, 0), 0.4)
		ensurePrompt(record, "Weld", "Breach")
		publishHullIntegrity()
		toastAll(("%s Hull breach: %s! Weld it shut!"):format(EMOJI_BREACH, part.Name))
	end

	local function ignite(record: PartRecord)
		local part = record.part
		if part:GetAttribute("State") ~= "Ok" then
			return
		end
		part:SetAttribute("State", "OnFire")
		part:SetAttribute("Damage", DamageConfig.FireDamage)
		part:SetAttribute("RepairKind", "Extinguisher")
		local fire = Instance.new("Fire")
		fire.Size = 8
		fire.Heat = 12
		fire.Parent = part
		record.fire = fire
		local smoke = Instance.new("Smoke")
		smoke.Size = 6
		smoke.Opacity = 0.35
		smoke.RiseVelocity = 8
		smoke.Parent = part
		record.smoke = smoke
		ensurePrompt(record, "Extinguish", "Engine fire")
		toastAll(("%s %s is on fire! Grab the extinguisher!"):format(EMOJI_FIRE, part.Name))
	end

	local function icePart(record: PartRecord)
		local part = record.part
		if part:GetAttribute("State") ~= "Ok" then
			return
		end
		part:SetAttribute("State", "Iced")
		part:SetAttribute("Damage", DamageConfig.IceDamage)
		part:SetAttribute("RepairKind", "IceAxe")
		part.Material = Enum.Material.Ice
		part.Color = Color3.fromRGB(200, 230, 240)
		ensurePrompt(record, "Hack Ice", "Ice sheet")
		publishIceMass()
		toastAll(("%s Ice is forming on %s! Hack it off!"):format(EMOJI_ICE, part.Name))
	end

	local function detachPart(record: PartRecord)
		local part = record.part
		part:SetAttribute("State", "Detached")
		part:SetAttribute("Damage", DamageConfig.DetachDamage)
		part:SetAttribute("RepairKind", "Hammer")
		record.crackedAt = nil
		clearEffects(record)

		-- Cosmetic flung panel; the real part just goes invisible.
		local ghost = part:Clone()
		ghost:ClearAllChildren()
		for _, tag in Refs.Tags do
			ghost:RemoveTag(tag)
		end
		ghost.Name = part.Name .. "_Debris"
		ghost.CanCollide = false
		ghost.Anchored = false
		ghost.Massless = false
		ghost.Parent = workspace
		local fling = Vector3.new(
			RNG:NextNumber(-1, 1),
			RNG:NextNumber(0.2, 1),
			RNG:NextNumber(-1, 1)
		).Unit * DamageConfig.DetachFlingSpeed
		ghost.AssemblyLinearVelocity = planeRoot.AssemblyLinearVelocity + fling
		ghost.AssemblyAngularVelocity =
			Vector3.new(RNG:NextNumber(-8, 8), RNG:NextNumber(-8, 8), RNG:NextNumber(-8, 8))
		Debris:AddItem(ghost, 6)

		part.Transparency = 1
		part.CanCollide = false
		if record.zone == "Engine" then
			part:SetAttribute("EngineHealth", 0)
			ensurePrompt(record, "Hammer", "Engine mount")
		else
			ensurePrompt(record, "Hammer", "Missing panel")
		end
		publishHullIntegrity()
		toastAll(("%s %s tore off the plane!"):format(EMOJI_BANG, part.Name))
	end

	-- Registry: every VE_Repairable part gets a record and clean attributes.
	Refs.onAdded(Refs.Tags.Repairable, function(instance)
		if not instance:IsA("BasePart") or records[instance] then
			return
		end
		local zone = instance:GetAttribute("DamageZone")
		records[instance] = {
			part = instance,
			zone = if typeof(zone) == "string" then zone else "Hull",
			originalColor = instance.Color,
			originalMaterial = instance.Material,
			originalTransparency = instance.Transparency,
			crackedAt = nil,
			prompt = nil,
			promptLabel = "",
			fire = nil,
			smoke = nil,
		}
		instance:SetAttribute("State", "Ok")
		instance:SetAttribute("Damage", 0)
	end)

	local function randomOkRecord(zone: string): PartRecord?
		local candidates = {}
		for _, record in records do
			if record.zone == zone and record.part:GetAttribute("State") == "Ok" then
				table.insert(candidates, record)
			end
		end
		if #candidates == 0 then
			return nil
		end
		return candidates[RNG:NextInteger(1, #candidates)]
	end

	-- Public API (mission scripts drive these directly in Phase 6).
	function DamageService.crackRandomHull()
		local record = randomOkRecord("Hull")
		if record then
			crackPart(record)
		end
	end

	function DamageService.igniteRandomEngine()
		local record = randomOkRecord("Engine")
		if record then
			ignite(record)
		end
	end

	function DamageService.iceRandomWing()
		local record = randomOkRecord("Wing")
		if record then
			icePart(record)
		end
	end

	function DamageService.lightningStrike()
		local record = randomOkRecord("Hull")
		local engine = randomOkRecord("Engine")
		local target = record or engine
		if not target then
			return
		end
		local point = target.part.Position
		local sky = point + Vector3.new(RNG:NextNumber(-30, 30), 140, RNG:NextNumber(-30, 30))
		local bolt = Instance.new("Part")
		bolt.Name = "LightningBolt"
		bolt.Material = Enum.Material.Neon
		bolt.Color = Color3.fromRGB(200, 220, 255)
		bolt.Anchored = true
		bolt.CanCollide = false
		bolt.Size = Vector3.new(0.5, 0.5, (sky - point).Magnitude)
		bolt.CFrame = CFrame.lookAt((sky + point) / 2, point)
		bolt.Parent = workspace
		Debris:AddItem(bolt, 0.35)

		toastAll(EMOJI_BOLT .. " Lightning strike!")
		if target == record then
			crackPart(target)
		else
			ignite(target)
		end
		if engine and RNG:NextNumber() < DamageConfig.LightningEngineFireChance then
			ignite(engine)
		end
	end

	-- Depressurization: tug loose characters toward the breach.
	local function pullCharactersToward(point: Vector3, speedDelta: number)
		local function tug(character: Model)
			local humanoid = character:FindFirstChildOfClass("Humanoid")
			local root = character:FindFirstChild("HumanoidRootPart")
			if not humanoid or not root or not root:IsA("BasePart") then
				return
			end
			if humanoid.Health <= 0 or humanoid.SeatPart ~= nil then
				return
			end
			local offset = point - root.Position
			local distance = offset.Magnitude
			if distance > DamageConfig.DepressurizationRadius or distance < 2.5 then
				return
			end
			root:ApplyImpulse(offset.Unit * speedDelta * root.AssemblyMass)
		end
		for _, player in Players:GetPlayers() do
			if player.Character then
				tug(player.Character)
			end
		end
		local passengersFolder = workspace:FindFirstChild("VE_Passengers")
		if passengersFolder then
			for _, child in passengersFolder:GetChildren() do
				if child:IsA("Model") then
					tug(child)
				end
			end
		end
	end

	-- Escalation tick.
	task.spawn(function()
		while true do
			task.wait(DamageConfig.EscalationTickSeconds)
			local now = os.clock()
			local airspeed = readNumber(plane, "Airspeed", 0)
			for _, record in records do
				local state = record.part:GetAttribute("State")
				if
					state == "Cracked"
					and record.crackedAt
					and now - record.crackedAt > DamageConfig.CrackWorsenSeconds
				then
					detachPart(record)
				elseif state == "OnFire" and record.zone == "Engine" then
					local health = math.max(
						0,
						readNumber(record.part, "EngineHealth", 100)
							- DamageConfig.FireBurnPerSecond
					)
					record.part:SetAttribute("EngineHealth", health)
					if health <= 0 then
						detachPart(record)
					end
				elseif state == "Ok" and record.zone == "Engine" then
					local health = readNumber(record.part, "EngineHealth", 100)
					if health < 100 then
						record.part:SetAttribute(
							"EngineHealth",
							math.min(100, health + DamageConfig.EngineRegenPerSecond)
						)
					end
				end
			end

			if airspeed >= DamageConfig.MinAirspeedForHazards then
				for _, record in records do
					if record.zone == "Hull" then
						local state = record.part:GetAttribute("State")
						if state == "Cracked" or state == "Detached" then
							local pull = if state == "Detached"
								then DamageConfig.DetachedPullSpeedPerSecond
								else DamageConfig.CrackedPullSpeedPerSecond
							pullCharactersToward(
								record.part.Position,
								pull * DamageConfig.EscalationTickSeconds
							)
						end
					end
				end
			end
		end
	end)

	-- Hazard scheduler (Phase 6 missions will drive the public API instead).
	task.spawn(function()
		while true do
			task.wait(
				RNG:NextNumber(
					DamageConfig.HazardMinIntervalSeconds,
					DamageConfig.HazardMaxIntervalSeconds
				)
			)
			if readNumber(plane, "Airspeed", 0) < DamageConfig.MinAirspeedForHazards then
				continue
			end
			local weights = DamageConfig.EventWeights
			local total = weights.HullCrack + weights.EngineFire + weights.Lightning + weights.Icing
			local roll = RNG:NextNumber(0, total)
			if roll < weights.HullCrack then
				DamageService.crackRandomHull()
			elseif roll < weights.HullCrack + weights.EngineFire then
				DamageService.igniteRandomEngine()
			elseif roll < weights.HullCrack + weights.EngineFire + weights.Lightning then
				DamageService.lightningStrike()
			else
				DamageService.iceRandomWing()
			end
		end
	end)

	publishHullIntegrity()
	publishIceMass()
end

return DamageService
