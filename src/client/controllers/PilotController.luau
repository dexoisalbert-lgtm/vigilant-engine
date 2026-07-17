--!strict
-- Reads pilot input while the local player occupies the VE_PilotSeat and
-- streams it to the server over the "Cockpit" NetBus channel.
--
--   W / S            pitch down / pull up   (p: +1 = pull up)
--   A / D            roll left / right      (r: +1 = bank right)
--   LShift / LCtrl   throttle up / down     (ramped locally, sent absolute)
--
-- Sends are rate-capped (FlightConfig.InputSendHz) with a keepalive so the
-- server's input-timeout watchdog doesn't zero the stick mid-maneuver.

local CollectionService = game:GetService("CollectionService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local FlightConfig = require(Shared:WaitForChild("FlightConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))

local PilotController = {}
PilotController.name = "PilotController"

local localPlayer = Players.LocalPlayer

function PilotController.start()
	local bus = NetBus.client("Cockpit")

	local piloting = false
	local throttle = 0
	local lastSentPitch = 0
	local lastSentRoll = 0
	local lastInputSentAt = 0
	local lastThrottleSentAt = 0
	local lastSentThrottle = -1

	local plane: Model? = nil
	task.spawn(function()
		plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	end)

	local function keyAxis(negative: Enum.KeyCode, positive: Enum.KeyCode): number
		local value = 0
		if UserInputService:IsKeyDown(positive) then
			value += 1
		end
		if UserInputService:IsKeyDown(negative) then
			value -= 1
		end
		return value
	end

	local function onSeated(active: boolean, seatPart: BasePart?)
		local wasPiloting = piloting
		piloting = active
			and seatPart ~= nil
			and CollectionService:HasTag(seatPart, Refs.Tags.PilotSeat)
		if piloting and plane then
			-- Adopt the live throttle so re-boarding doesn't slam it to zero.
			local current = plane:GetAttribute("EngineThrottle")
			throttle = if typeof(current) == "number" then current else 0
		elseif wasPiloting and not piloting then
			bus.fire("Input", { p = 0, r = 0 })
			lastSentPitch, lastSentRoll = 0, 0
		end
	end

	local function hookCharacter(character: Model)
		local humanoid = character:WaitForChild("Humanoid") :: Humanoid
		humanoid.Seated:Connect(onSeated)
	end

	if localPlayer.Character then
		task.spawn(hookCharacter, localPlayer.Character)
	end
	localPlayer.CharacterAdded:Connect(hookCharacter)

	RunService.Heartbeat:Connect(function(dt: number)
		if not piloting then
			return
		end

		local typing = UserInputService:GetFocusedTextBox() ~= nil
		local pitch = if typing then 0 else keyAxis(Enum.KeyCode.W, Enum.KeyCode.S)
		local roll = if typing then 0 else keyAxis(Enum.KeyCode.A, Enum.KeyCode.D)

		local now = os.clock()
		local changed = pitch ~= lastSentPitch or roll ~= lastSentRoll
		local sinceSend = now - lastInputSentAt
		if
			(changed and sinceSend >= 1 / FlightConfig.InputSendHz)
			or sinceSend >= FlightConfig.InputKeepaliveSeconds
		then
			bus.fire("Input", { p = pitch, r = roll })
			lastSentPitch, lastSentRoll = pitch, roll
			lastInputSentAt = now
		end

		if not typing then
			local throttleAxis = keyAxis(Enum.KeyCode.LeftControl, Enum.KeyCode.LeftShift)
			if throttleAxis ~= 0 then
				throttle =
					math.clamp(throttle + throttleAxis * FlightConfig.ThrottleChangeRate * dt, 0, 1)
			end
		end
		if math.abs(throttle - lastSentThrottle) > 0.004 and now - lastThrottleSentAt >= 0.1 then
			bus.fire("Throttle", { t = throttle })
			lastSentThrottle = throttle
			lastThrottleSentAt = now
		end
	end)
end

return PilotController
