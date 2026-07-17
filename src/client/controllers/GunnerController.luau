--!strict
-- Streams gatling fire input while the local player occupies a VE_GatlingSeat.
-- Hold Mouse Button 1 or F (a touch button appears on mobile) to fire; the
-- aim direction is the camera look vector, validated and resolved server-side.

local CollectionService = game:GetService("CollectionService")
local ContextActionService = game:GetService("ContextActionService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local CombatConfig = require(Shared:WaitForChild("CombatConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))

local GunnerController = {}
GunnerController.name = "GunnerController"

local localPlayer = Players.LocalPlayer
local FIRE_ACTION = "VE_GatlingFire"

function GunnerController.start()
	local bus = NetBus.client("Combat")

	local gunning = false
	local firing = false
	local lastSentAt = 0

	local function fireAction(_name: string, state: Enum.UserInputState): Enum.ContextActionResult
		if state == Enum.UserInputState.Begin then
			firing = true
		elseif state == Enum.UserInputState.End or state == Enum.UserInputState.Cancel then
			firing = false
		end
		return Enum.ContextActionResult.Sink
	end

	local function onSeated(active: boolean, seatPart: BasePart?)
		local nowGunning = active
			and seatPart ~= nil
			and CollectionService:HasTag(seatPart, Refs.Tags.GatlingSeat)
		if nowGunning == gunning then
			return
		end
		gunning = nowGunning
		firing = false
		if gunning then
			ContextActionService:BindAction(
				FIRE_ACTION,
				fireAction,
				true,
				Enum.UserInputType.MouseButton1,
				Enum.KeyCode.F
			)
			ContextActionService:SetTitle(FIRE_ACTION, "FIRE")
		else
			ContextActionService:UnbindAction(FIRE_ACTION)
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

	RunService.Heartbeat:Connect(function()
		if not gunning or not firing then
			return
		end
		local now = os.clock()
		if now - lastSentAt < CombatConfig.GatlingFireCooldownSeconds then
			return
		end
		local camera = workspace.CurrentCamera
		if not camera then
			return
		end
		lastSentAt = now
		bus.fire("Fire", { dir = camera.CFrame.LookVector })
	end)
end

return GunnerController
