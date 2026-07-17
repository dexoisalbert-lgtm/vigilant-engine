--!strict
-- Server-authoritative arcade flight model (Phase 1, one plane per server).
--
-- Inputs arrive from the pilot's client over the "Cockpit" NetBus channel
-- (never via VehicleSeat.Throttle/Steer, which don't replicate reliably under
-- server network ownership). Every Heartbeat this service:
--   * integrates desired pitch/roll/yaw from the pilot's stick,
--   * drives the AlignOrientation goal and the thrust/lift/drag VectorForces,
--   * burns fuel and publishes instrument attributes on the plane model.
--
-- Force model: thrust = mass * MaxForwardAccel * throttle * engineFactor along
-- -Z; lift = mass * gravity * min(forwardSpeed/StallSpeed, 1) along body up
-- (level flight is neutral above stall, sinking below it); drag = -velocity *
-- mass * (MaxForwardAccel/MaxSpeed), so full throttle tops out at MaxSpeed.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local FlightConfig = require(Shared:WaitForChild("FlightConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))

local FlightController = {}
FlightController.name = "FlightController"

-- Moves current toward target by at most maxDelta.
local function approach(current: number, target: number, maxDelta: number): number
	return current + math.clamp(target - current, -maxDelta, maxDelta)
end

local function isFiniteNumber(value: unknown): boolean
	return typeof(value) == "number" and value == value and math.abs(value) ~= math.huge
end

function FlightController.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local root = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart
	local seat = Refs.waitFor(Refs.Tags.PilotSeat) :: VehicleSeat
	local dash = Refs.waitFor(Refs.Tags.CockpitDash) :: BasePart

	local thrustForce = root:WaitForChild("VE_ThrustForce") :: VectorForce
	local liftForce = root:WaitForChild("VE_LiftForce") :: VectorForce
	local dragForce = root:WaitForChild("VE_DragForce") :: VectorForce
	local align = root:WaitForChild("VE_Align") :: AlignOrientation

	-- Control state (radians).
	local desiredPitch = 0
	local desiredRoll = 0
	local look = root.CFrame.LookVector
	local yaw = math.atan2(-look.X, -look.Z)
	local inputPitch = 0
	local inputRoll = 0
	local lastInputAt = 0
	local apTargetAltitude: number? = nil
	local fuelExact = plane:GetAttribute("FuelKg") :: number

	local lastAttrs: { [string]: any } = {}
	local function publish(name: string, value: any)
		if lastAttrs[name] ~= value then
			lastAttrs[name] = value
			plane:SetAttribute(name, value)
		end
	end

	local function getPilot(): Player?
		local humanoid = seat.Occupant
		if not humanoid then
			return nil
		end
		local character = humanoid.Parent
		if not character then
			return nil
		end
		return Players:GetPlayerFromCharacter(character)
	end

	local bus = NetBus.server("Cockpit")

	bus.on("Input", function(player, data)
		if not data or player ~= getPilot() then
			return
		end
		local p, r = data.p, data.r
		if not isFiniteNumber(p) or not isFiniteNumber(r) then
			return
		end
		inputPitch = math.clamp(p, -1, 1)
		inputRoll = math.clamp(r, -1, 1)
		lastInputAt = os.clock()
	end)

	bus.on("Throttle", function(player, data)
		if not data or player ~= getPilot() then
			return
		end
		local t = data.t
		if not isFiniteNumber(t) then
			return
		end
		publish("EngineThrottle", math.clamp(t, 0, 1))
	end)

	-- External refuels (MissionService between missions) set the FuelKg
	-- attribute; adopt any value that differs beyond our own rounding.
	plane:GetAttributeChangedSignal("FuelKg"):Connect(function()
		local value = plane:GetAttribute("FuelKg")
		if typeof(value) == "number" and math.abs(value - fuelExact) > 2 then
			fuelExact = value
		end
	end)

	-- Thrust wash particles track the published throttle.
	plane:GetAttributeChangedSignal("EngineThrottle"):Connect(function()
		local throttleNow = plane:GetAttribute("EngineThrottle")
		if typeof(throttleNow) ~= "number" then
			return
		end
		for _, engine in Refs.all(Refs.Tags.Engine) do
			local emitter = engine:FindFirstChild("VE_ThrustEmitter")
			if emitter and emitter:IsA("ParticleEmitter") then
				emitter.Enabled = throttleNow > 0.05
				emitter.Rate = 10 + 60 * throttleNow
			end
		end
	end)

	local autopilotPrompt = dash:FindFirstChildOfClass("ProximityPrompt")
	if autopilotPrompt then
		autopilotPrompt.Triggered:Connect(function()
			local turningOn = plane:GetAttribute("AutopilotOn") ~= true
			apTargetAltitude = if turningOn then root.Position.Y else nil
			publish("AutopilotOn", turningOn)
		end)
	end

	local groundParams = RaycastParams.new()
	groundParams.FilterType = Enum.RaycastFilterType.Exclude
	groundParams.FilterDescendantsInstances = { plane }

	local function isGrounded(): boolean
		local probe = Vector3.new(0, -(root.Size.Y / 2 + FlightConfig.GroundProbeDepth), 0)
		return workspace:Raycast(root.Position, probe, groundParams) ~= nil
	end

	local function engineFactor(): number
		local engines = Refs.all(Refs.Tags.Engine)
		if #engines == 0 then
			return 1
		end
		local total = 0
		for _, engine in engines do
			local health = engine:GetAttribute("EngineHealth")
			total += math.clamp(if typeof(health) == "number" then health else 100, 0, 100)
		end
		return total / (#engines * 100)
	end

	local maxPitch = math.rad(FlightConfig.MaxPitchDeg)
	local maxRoll = math.rad(FlightConfig.MaxRollDeg)

	RunService.Heartbeat:Connect(function(dt: number)
		local mass = root.AssemblyMass
		local velocity = root.AssemblyLinearVelocity
		local forwardSpeed = velocity:Dot(root.CFrame.LookVector)
		local grounded = isGrounded()

		-- Zero the stick if the pilot left or their input stream went quiet.
		if not getPilot() or (os.clock() - lastInputAt) > FlightConfig.InputTimeoutSeconds then
			inputPitch = 0
			inputRoll = 0
		end

		local throttle = plane:GetAttribute("EngineThrottle") :: number
		if fuelExact <= 0 then
			throttle = 0
		end

		local pitchRate = math.rad(FlightConfig.PitchRateDeg) * dt
		local rollRate = math.rad(FlightConfig.RollRateDeg) * dt
		local levelRate = math.rad(FlightConfig.LevelDecayDeg) * dt

		if plane:GetAttribute("AutopilotOn") == true then
			-- Altitude + wings-level hold.
			local targetAlt = apTargetAltitude or root.Position.Y
			local command = math.clamp(
				(targetAlt - root.Position.Y) * FlightConfig.AutopilotAltGain
					- velocity.Y * FlightConfig.AutopilotVsGain,
				-1,
				1
			) * math.rad(FlightConfig.AutopilotMaxPitchDeg)
			desiredPitch = approach(desiredPitch, command, pitchRate)
			desiredRoll = approach(desiredRoll, 0, rollRate)
		elseif grounded then
			-- Ground handling: stay level, nosewheel-steer with the roll stick,
			-- and only allow rotation once fast enough.
			yaw -= inputRoll * math.rad(FlightConfig.GroundSteerRateDeg) * dt
			desiredRoll = approach(desiredRoll, 0, rollRate)
			if forwardSpeed >= FlightConfig.RotateSpeed and inputPitch > 0 then
				desiredPitch = math.clamp(desiredPitch + inputPitch * pitchRate, 0, maxPitch)
			else
				desiredPitch = approach(desiredPitch, 0, pitchRate)
			end
		else
			if inputPitch ~= 0 then
				desiredPitch =
					math.clamp(desiredPitch + inputPitch * pitchRate, -maxPitch, maxPitch)
			else
				desiredPitch = approach(desiredPitch, 0, levelRate)
			end
			if inputRoll ~= 0 then
				-- +1 stick = bank right = negative roll angle (up tilts toward +X).
				desiredRoll = math.clamp(desiredRoll - inputRoll * rollRate, -maxRoll, maxRoll)
			else
				desiredRoll = approach(desiredRoll, 0, levelRate)
			end
			-- Banking drives the turn: positive (left) roll yaws left.
			yaw += (desiredRoll / maxRoll) * math.rad(FlightConfig.TurnRateDegAtFullBank) * dt
		end
		yaw %= 2 * math.pi

		align.CFrame = CFrame.fromEulerAnglesYXZ(desiredPitch, yaw, desiredRoll)

		local effectiveThrottle = throttle * engineFactor()
		thrustForce.Force =
			Vector3.new(0, 0, -mass * FlightConfig.MaxForwardAccel * effectiveThrottle)

		-- Ice and excess cargo are virtual weight the wings have to carry:
		-- both subtract straight from lift (published by DamageService and
		-- CargoService respectively, already in effective mass units).
		local iceMass = plane:GetAttribute("IceMass")
		local excessCargo = plane:GetAttribute("ExcessCargoMass")
		local deadMass = (if typeof(iceMass) == "number" then iceMass else 0)
			+ (if typeof(excessCargo) == "number" then excessCargo else 0)
		local liftFraction = math.clamp(forwardSpeed / FlightConfig.StallSpeed, 0, 1)
		liftForce.Force = Vector3.new(0, (mass * liftFraction - deadMass) * workspace.Gravity, 0)

		local dragCoefficient = FlightConfig.MaxForwardAccel / FlightConfig.MaxSpeed
		dragForce.Force = -velocity * mass * dragCoefficient

		if effectiveThrottle > 0 then
			fuelExact = math.max(0, fuelExact - FlightConfig.FuelBurnKgPerSec * throttle * dt)
		end

		-- Instruments (rounded so attribute replication only fires on change).
		publish("Altitude", math.round(root.Position.Y))
		publish("Airspeed", math.round(math.max(forwardSpeed, 0)))
		publish("VerticalSpeed", math.round(velocity.Y))
		publish("HeadingDeg", math.round(math.deg(-yaw) % 360))
		publish("FuelKg", math.round(fuelExact))
	end)
end

return FlightController
