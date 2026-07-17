--!strict
-- Demand side of cabin service (Phase 2): spawns passenger NPCs into
-- VE_CabinSeat seats and runs their state machine —
--   Seated -> Wanting -> (served: Seated | ignored: Enraged) -> Returning
-- plus Ragdolled via turbulence. Also owns seatbelt buckling and the
-- periodic turbulence scheduler that tosses every unbuckled occupant.
--
-- Buckling is a flag on the seat ("Buckled" attribute): buckled occupants are
-- skipped by turbulence unseating; unbuckled ones get Sit=false plus a random
-- impulse. Impulses are task.defer-ed so the seat weld is gone before we read
-- AssemblyMass — otherwise the impulse would hit the whole plane assembly.

local CollectionService = game:GetService("CollectionService")
local Debris = game:GetService("Debris")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local CabinConfig = require(Shared:WaitForChild("CabinConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))
local RagdollUtil = require(Shared:WaitForChild("RagdollUtil"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any

local PassengerService = {}
PassengerService.name = "PassengerService"

local RNG = Random.new()
local TICK = 0.2

local EMOJI_ANGRY = "\u{1F621}"
local EMOJI_DIZZY = "\u{1F635}"
local EMOJI_HAPPY = "\u{1F60A}"
local EMOJI_PLANE = "\u{2708}\u{FE0F}"

local COLOR_BUBBLE = Color3.fromRGB(255, 255, 255)
local COLOR_BUBBLE_ANGRY = Color3.fromRGB(255, 80, 70)

local function randomDescription(): HumanoidDescription
	local desc = Instance.new("HumanoidDescription")
	local skin = CabinConfig.SkinTones[RNG:NextInteger(1, #CabinConfig.SkinTones)]
	local shirt = CabinConfig.ShirtColors[RNG:NextInteger(1, #CabinConfig.ShirtColors)]
	local pants = CabinConfig.PantsColors[RNG:NextInteger(1, #CabinConfig.PantsColors)]
	desc.HeadColor = skin
	desc.LeftArmColor = skin
	desc.RightArmColor = skin
	desc.TorsoColor = shirt
	desc.LeftLegColor = pants
	desc.RightLegColor = pants
	return desc
end

-- Legless capsule passenger, used only if the rig builder API fails.
local function makeFallbackRig(): Model
	local model = Instance.new("Model")
	local rootPart = Instance.new("Part")
	rootPart.Name = "HumanoidRootPart"
	rootPart.Size = Vector3.new(2, 2, 1)
	rootPart.Transparency = 1
	rootPart.CanCollide = false
	rootPart.Parent = model

	local torso = Instance.new("Part")
	torso.Name = "Torso"
	torso.Size = Vector3.new(2, 2, 1)
	torso.Color = CabinConfig.ShirtColors[RNG:NextInteger(1, #CabinConfig.ShirtColors)]
	torso.Parent = model
	local torsoWeld = Instance.new("WeldConstraint")
	torsoWeld.Part0 = rootPart
	torsoWeld.Part1 = torso
	torsoWeld.Parent = torso

	local head = Instance.new("Part")
	head.Name = "Head"
	head.Shape = Enum.PartType.Ball
	head.Size = Vector3.new(1.2, 1.2, 1.2)
	head.Color = CabinConfig.SkinTones[RNG:NextInteger(1, #CabinConfig.SkinTones)]
	head.CFrame = CFrame.new(0, 1.6, 0)
	head.Parent = model
	local headWeld = Instance.new("WeldConstraint")
	headWeld.Part0 = rootPart
	headWeld.Part1 = head
	headWeld.Parent = head

	local humanoid = Instance.new("Humanoid")
	humanoid.RequiresNeck = false
	humanoid.HipHeight = 1
	humanoid.Parent = model
	model.PrimaryPart = rootPart
	return model
end

local function makeRig(): Model
	local model: Model? = nil
	local ok, result = pcall(function()
		return Players:CreateHumanoidModelFromDescription(
			randomDescription(),
			Enum.HumanoidRigType.R15
		)
	end)
	if ok and result then
		model = result
	else
		model = makeFallbackRig()
	end
	return model :: Model
end

-- Public: other services (MissionService's air marshal) reuse the NPC rig
-- builder rather than duplicating the humanoid-construction fallbacks.
function PassengerService.makeNpcRig(): Model
	return makeRig()
end

type PassengerState = {
	model: Model,
	humanoid: Humanoid,
	root: BasePart,
	seat: Seat,
	displayName: string,
	state: string, -- "Seated" | "Wanting" | "Enraged" | "Ragdolled" | "Returning"
	stateEpoch: number,
	wantItem: string?,
	timer: number,
	moveAt: number,
	shoveAt: number,
	dead: boolean,
	bubbleGui: BillboardGui,
	bubbleLabel: TextLabel,
	servePrompt: ProximityPrompt,
}

function PassengerService.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local planeRoot = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart
	local bus = NetBus.server("Cabin")

	local passengers: { PassengerState } = {}
	local nameIndex = 0

	local passengersFolder = Instance.new("Folder")
	passengersFolder.Name = "VE_Passengers"
	passengersFolder.Parent = workspace

	local function insideCabin(position: Vector3): boolean
		-- Covers the cabin plus the aft cargo bay (which ends at z ~38).
		local rel = planeRoot.CFrame:PointToObjectSpace(position)
		return math.abs(rel.X) <= 5.5
			and rel.Y >= -1.5
			and rel.Y <= 9.5
			and rel.Z >= -23.5
			and rel.Z <= 38.5
	end

	local function setState(st: PassengerState, newState: string)
		st.stateEpoch += 1
		st.state = newState
		if newState == "Seated" then
			st.wantItem = nil
			st.timer = RNG:NextNumber(CabinConfig.DemandMinSeconds, CabinConfig.DemandMaxSeconds)
			st.servePrompt.Enabled = false
			st.bubbleGui.Enabled = false
		elseif newState == "Wanting" then
			local item = CabinConfig.Items[RNG:NextInteger(1, #CabinConfig.Items)]
			st.wantItem = item.id
			st.timer = CabinConfig.PatienceSeconds
			st.bubbleLabel.Text = ("%s %s!"):format(item.emoji, item.id)
			st.bubbleLabel.TextColor3 = COLOR_BUBBLE
			st.bubbleGui.Enabled = true
			st.servePrompt.Enabled = true
		elseif newState == "Enraged" then
			st.wantItem = nil
			st.timer = CabinConfig.EnrageSeconds
			st.moveAt = 0
			st.servePrompt.Enabled = false
			st.bubbleLabel.Text = EMOJI_ANGRY
			st.bubbleLabel.TextColor3 = COLOR_BUBBLE_ANGRY
			st.bubbleGui.Enabled = true
			st.seat:SetAttribute("Buckled", false)
			st.humanoid.Sit = false
		elseif newState == "Ragdolled" then
			st.timer = CabinConfig.RagdollSeconds
			st.servePrompt.Enabled = false
			st.bubbleLabel.Text = EMOJI_DIZZY
			st.bubbleLabel.TextColor3 = COLOR_BUBBLE
			st.bubbleGui.Enabled = true
			st.humanoid.Sit = false
			RagdollUtil.ragdoll(st.model, CabinConfig.RagdollSeconds)
		elseif newState == "Returning" then
			st.timer = CabinConfig.ReturnTimeoutSeconds
			st.moveAt = 0
			st.humanoid.PlatformStand = false
			st.bubbleGui.Enabled = false
		end
	end

	-- Random impulse once the seat weld is verifiably gone (see file header).
	local function deferImpulse(humanoid: Humanoid, root: BasePart, scale: number)
		task.defer(function()
			if humanoid.SeatPart == nil and humanoid.Health > 0 then
				local dir = Vector3.new(RNG:NextNumber(-1, 1), 0, RNG:NextNumber(-1, 1))
				dir = if dir.Magnitude > 0.05 then dir.Unit else Vector3.xAxis
				local velocity = dir * CabinConfig.TurbulenceTossSpeed * scale
					+ Vector3.new(0, CabinConfig.TurbulenceUpSpeed * scale, 0)
				root:ApplyImpulse(velocity * root.AssemblyMass)
			end
		end)
	end

	local function spawnPassenger(seat: Seat)
		if seat.Occupant then
			return
		end
		nameIndex += 1
		local displayName =
			CabinConfig.PassengerNames[(nameIndex - 1) % #CabinConfig.PassengerNames + 1]

		local model = makeRig()
		model.Name = "Passenger_" .. displayName
		local humanoid = model:FindFirstChildOfClass("Humanoid") :: Humanoid
		humanoid.DisplayName = displayName
		humanoid.WalkSpeed = CabinConfig.PassengerWalkSpeed
		model.Parent = passengersFolder
		model:PivotTo(seat.CFrame * CFrame.new(0, 3, 0))

		local root = model:FindFirstChild("HumanoidRootPart") :: BasePart
		pcall(function()
			root:SetNetworkOwner(nil)
		end)
		seat:Sit(humanoid)

		local head = (model:FindFirstChild("Head") or root) :: BasePart

		local bubbleGui = Instance.new("BillboardGui")
		bubbleGui.Name = "DemandBubble"
		bubbleGui.Size = UDim2.new(0, 140, 0, 42)
		bubbleGui.StudsOffset = Vector3.new(0, 2.2, 0)
		bubbleGui.AlwaysOnTop = true
		bubbleGui.MaxDistance = 80
		bubbleGui.Enabled = false
		local bubbleLabel = Instance.new("TextLabel")
		bubbleLabel.Size = UDim2.fromScale(1, 1)
		bubbleLabel.BackgroundColor3 = Color3.fromRGB(12, 14, 16)
		bubbleLabel.BackgroundTransparency = 0.35
		bubbleLabel.Font = Enum.Font.GothamBold
		bubbleLabel.TextScaled = true
		bubbleLabel.TextColor3 = COLOR_BUBBLE
		bubbleLabel.Text = ""
		bubbleLabel.Parent = bubbleGui
		bubbleGui.Parent = head

		local servePrompt = Instance.new("ProximityPrompt")
		servePrompt.Name = "ServePrompt"
		servePrompt.ActionText = "Serve"
		servePrompt.ObjectText = displayName
		servePrompt.HoldDuration = 0.4
		servePrompt.MaxActivationDistance = 7
		servePrompt.RequiresLineOfSight = false
		servePrompt.Enabled = false
		servePrompt.Parent = head

		local st: PassengerState = {
			model = model,
			humanoid = humanoid,
			root = root,
			seat = seat,
			displayName = displayName,
			state = "Seated",
			stateEpoch = 0,
			wantItem = nil,
			timer = 0,
			moveAt = 0,
			shoveAt = 0,
			dead = false,
			bubbleGui = bubbleGui,
			bubbleLabel = bubbleLabel,
			servePrompt = servePrompt,
		}
		table.insert(passengers, st)

		servePrompt.Triggered:Connect(function(player)
			if st.dead or st.state ~= "Wanting" then
				return
			end
			local character = player.Character
			local tool = character and character:FindFirstChildOfClass("Tool")
			local contents = tool and tool:GetAttribute("Contents")
			if tool and typeof(contents) == "string" and contents == st.wantItem then
				tool:Destroy()
				ScoreService.award(player, CabinConfig.ServeScore)
				bus.fire(player, "Toast", {
					text = ("+%d  %s enjoyed the %s"):format(
						CabinConfig.ServeScore,
						st.displayName,
						contents
					),
				})
				setState(st, "Seated")
				st.bubbleLabel.Text = EMOJI_HAPPY
				st.bubbleLabel.TextColor3 = COLOR_BUBBLE
				st.bubbleGui.Enabled = true
				local epoch = st.stateEpoch
				task.delay(1.5, function()
					if st.stateEpoch == epoch then
						st.bubbleGui.Enabled = false
					end
				end)
			else
				bus.fire(player, "Toast", {
					text = ("%s wants a %s!"):format(st.displayName, st.wantItem or "?"),
				})
			end
		end)

		humanoid.Died:Connect(function()
			st.dead = true
			Debris:AddItem(model, 4)
			task.delay(CabinConfig.PassengerRespawnSeconds, function()
				if seat.Parent and not seat.Occupant then
					spawnPassenger(seat)
				end
			end)
		end)

		setState(st, "Seated")
		st.timer =
			RNG:NextNumber(CabinConfig.FirstDemandMinSeconds, CabinConfig.FirstDemandMaxSeconds)
	end

	-- Seatbelt prompts (players buckle themselves or any seated passenger).
	Refs.onAdded(Refs.Tags.CabinSeat, function(instance)
		local seat = instance :: Seat
		local prompt = seat:FindFirstChild("BucklePrompt")
		if not prompt or not prompt:IsA("ProximityPrompt") then
			return
		end
		prompt.Triggered:Connect(function()
			if not seat.Occupant then
				return
			end
			local wasBuckled = seat:GetAttribute("Buckled") == true
			seat:SetAttribute("Buckled", not wasBuckled)
			prompt.ActionText = if wasBuckled then "Buckle Seatbelt" else "Unbuckle"
		end)
		seat:GetPropertyChangedSignal("Occupant"):Connect(function()
			if not seat.Occupant then
				seat:SetAttribute("Buckled", false)
				prompt.ActionText = "Buckle Seatbelt"
			end
		end)
	end)

	-- Board the passengers.
	local seats = {}
	for _, instance in Refs.all(Refs.Tags.CabinSeat) do
		if instance:IsA("Seat") then
			table.insert(seats, instance)
		end
	end
	for index = #seats, 2, -1 do
		local swap = RNG:NextInteger(1, index)
		seats[index], seats[swap] = seats[swap], seats[index]
	end
	for index = 1, math.min(CabinConfig.PassengerCount, #seats) do
		spawnPassenger(seats[index])
	end

	local function randomCabinPoint(): Vector3
		return planeRoot.CFrame:PointToWorldSpace(
			Vector3.new(RNG:NextNumber(-3, 3), 1.5, RNG:NextNumber(-12, 14))
		)
	end

	-- Passenger state machine tick.
	task.spawn(function()
		while true do
			task.wait(TICK)
			for index = #passengers, 1, -1 do
				if passengers[index].dead then
					table.remove(passengers, index)
				end
			end
			local now = os.clock()
			for _, st in passengers do
				if st.humanoid.Health <= 0 then
					continue
				end
				st.timer -= TICK

				-- Fell out of the plane (and isn't welded into a seat).
				if st.humanoid.SeatPart == nil and not insideCabin(st.root.Position) then
					st.humanoid.Health = 0
					continue
				end

				if st.state == "Seated" then
					if st.timer <= 0 then
						setState(st, "Wanting")
					end
				elseif st.state == "Wanting" then
					local urgency = 1 - math.max(st.timer, 0) / CabinConfig.PatienceSeconds
					st.bubbleLabel.TextColor3 = COLOR_BUBBLE:Lerp(COLOR_BUBBLE_ANGRY, urgency)
					if st.timer <= 0 then
						setState(st, "Enraged")
						bus.fireAll("Toast", { text = ("%s is furious!"):format(st.displayName) })
					end
				elseif st.state == "Enraged" then
					if now >= st.moveAt then
						st.humanoid:MoveTo(randomCabinPoint())
						st.moveAt = now + 1.5
					end
					if now >= st.shoveAt then
						for _, player in Players:GetPlayers() do
							local character = player.Character
							local humanoid = character
								and character:FindFirstChildOfClass("Humanoid")
							local root = character
								and character:FindFirstChild("HumanoidRootPart") :: BasePart?
							if
								humanoid
								and root
								and humanoid.Health > 0
								and humanoid.SeatPart == nil
								and (root.Position - st.root.Position).Magnitude
									<= CabinConfig.EnrageShoveRadius
							then
								local away = root.Position - st.root.Position
								local flat = Vector3.new(away.X, 0, away.Z)
								flat = if flat.Magnitude > 0.05 then flat.Unit else Vector3.xAxis
								local velocity = flat * CabinConfig.EnrageShoveSpeed
									+ Vector3.new(0, 8, 0)
								root:ApplyImpulse(velocity * root.AssemblyMass)
								st.shoveAt = now + CabinConfig.EnrageShoveCooldownSeconds
								break
							end
						end
					end
					if st.timer <= 0 then
						setState(st, "Returning")
					end
				elseif st.state == "Ragdolled" then
					if st.timer <= 0 then
						setState(st, "Returning")
					end
				elseif st.state == "Returning" then
					if st.humanoid.SeatPart == st.seat then
						setState(st, "Seated")
					elseif st.timer <= 0 then
						-- Walking home failed (moving plane); just put them back.
						st.model:PivotTo(st.seat.CFrame * CFrame.new(0, 3, 0))
						st.seat:Sit(st.humanoid)
						setState(st, "Seated")
					elseif now >= st.moveAt and st.seat.Occupant == nil then
						st.humanoid:MoveTo(st.seat.Position)
						st.moveAt = now + 1
					elseif st.seat.Occupant ~= nil then
						-- Someone stole the seat; wander until it frees up.
						if now >= st.moveAt then
							st.humanoid:MoveTo(randomCabinPoint())
							st.moveAt = now + 2
						end
						st.timer = math.max(st.timer, 2)
					end
				end
			end
		end
	end)

	-- Turbulence scheduler.
	task.spawn(function()
		while true do
			task.wait(
				RNG:NextNumber(
					CabinConfig.TurbulenceMinIntervalSeconds,
					CabinConfig.TurbulenceMaxIntervalSeconds
				)
			)
			local airspeed = plane:GetAttribute("Airspeed")
			if typeof(airspeed) ~= "number" or airspeed < CabinConfig.TurbulenceMinAirspeed then
				continue
			end

			bus.fireAll("Turbulence", { d = CabinConfig.TurbulenceDurationSeconds })
			bus.fireAll("Toast", { text = EMOJI_PLANE .. " Turbulence! Buckle up!" })

			for pulse = 1, 3 do
				-- NPCs: unbuckled seated ones get unseated and tossed.
				for _, st in passengers do
					if st.dead or st.humanoid.Health <= 0 then
						continue
					end
					local seated = st.humanoid.SeatPart ~= nil
					local buckled = st.seat:GetAttribute("Buckled") == true
					if seated and buckled then
						continue
					end
					if seated or st.state == "Enraged" or st.state == "Returning" then
						setState(st, "Ragdolled")
						deferImpulse(st.humanoid, st.root, 1)
					end
				end

				-- Players: same rules; the pilot seat counts as buckled.
				for _, player in Players:GetPlayers() do
					local character = player.Character
					local humanoid = character and character:FindFirstChildOfClass("Humanoid")
					local root = character
						and character:FindFirstChild("HumanoidRootPart") :: BasePart?
					if not character or not humanoid or not root or humanoid.Health <= 0 then
						continue
					end
					if not insideCabin(root.Position) then
						continue
					end
					local seatPart = humanoid.SeatPart
					if seatPart then
						if CollectionService:HasTag(seatPart, Refs.Tags.PilotSeat) then
							continue
						end
						if seatPart:GetAttribute("Buckled") == true then
							continue
						end
						humanoid.Sit = false
						deferImpulse(humanoid, root, 1)
						bus.fire(player, "Toast", { text = "Buckle up next time!" })
					else
						deferImpulse(humanoid, root, 0.6)
					end
				end

				task.wait(CabinConfig.TurbulenceDurationSeconds / 3)
			end
		end
	end)
end

return PassengerService
