--!strict
-- Wing-walk harness: while the local character stands on the plane's EXTERIOR
-- (wings, roof — outside the cabin interior bounds), carry them along with the
-- plane's frame-to-frame motion so banking and acceleration don't slide them
-- off. Runs client-side because the client owns its character's physics.
--
-- The carry is a soft lerp (CombatConfig.WingStickStrength) between native
-- physics and a hard lock to the airframe; inside the cabin native floor
-- friction is enough, so the harness only engages outside.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local CombatConfig = require(Shared:WaitForChild("CombatConfig"))

local WingWalkController = {}
WingWalkController.name = "WingWalkController"

local localPlayer = Players.LocalPlayer

function WingWalkController.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local planeRoot = Refs.waitFor(Refs.Tags.PlanePrimary) :: BasePart

	local lastPlaneCf: CFrame? = nil

	local function isOutsideCabin(position: Vector3): boolean
		local rel = planeRoot.CFrame:PointToObjectSpace(position)
		return math.abs(rel.X) > 5 or rel.Y > 9 or rel.Z > 38 or rel.Z < -24
	end

	RunService.Heartbeat:Connect(function()
		local planeCf = planeRoot.CFrame
		local previous = lastPlaneCf
		lastPlaneCf = planeCf
		if not previous then
			return
		end

		local character = localPlayer.Character
		local humanoid = character and character:FindFirstChildOfClass("Humanoid")
		local root = character and character:FindFirstChild("HumanoidRootPart")
		if
			not character
			or not humanoid
			or not root
			or not root:IsA("BasePart")
			or humanoid.Health <= 0
			or humanoid.SeatPart ~= nil
		then
			return
		end
		if not isOutsideCabin(root.Position) then
			return
		end

		-- Standing on the plane? (short raycast below the character's feet)
		local params = RaycastParams.new()
		params.FilterType = Enum.RaycastFilterType.Exclude
		params.FilterDescendantsInstances = { character }
		local hit = workspace:Raycast(root.Position, Vector3.new(0, -4, 0), params)
		if not hit or not hit.Instance:IsDescendantOf(plane) then
			return
		end

		-- Carry the character along with the plane's motion since last frame.
		local delta = planeCf * previous:Inverse()
		local target = delta * root.CFrame
		root.CFrame = root.CFrame:Lerp(target, CombatConfig.WingStickStrength)
	end)
end

return WingWalkController
