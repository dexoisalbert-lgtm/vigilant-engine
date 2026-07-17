--!strict
-- Locks the camera to first person while the local player occupies the
-- pilot seat, restoring free (classic) camera on exit.

local CollectionService = game:GetService("CollectionService")
local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))

local PilotCameraController = {}
PilotCameraController.name = "PilotCameraController"

local localPlayer = Players.LocalPlayer

function PilotCameraController.start()
	local function onSeated(active: boolean, seatPart: BasePart?)
		local isPilotSeat = active
			and seatPart ~= nil
			and CollectionService:HasTag(seatPart, Refs.Tags.PilotSeat)
		localPlayer.CameraMode = if isPilotSeat
			then Enum.CameraMode.LockFirstPerson
			else Enum.CameraMode.Classic
	end

	local function hookCharacter(character: Model)
		local humanoid = character:WaitForChild("Humanoid") :: Humanoid
		humanoid.Seated:Connect(onSeated)
	end

	if localPlayer.Character then
		task.spawn(hookCharacter, localPlayer.Character)
	end
	localPlayer.CharacterAdded:Connect(hookCharacter)
end

return PilotCameraController
