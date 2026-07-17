--!strict
-- Temporary ragdoll (Phase 7 ragdoll pass): swaps a rig's Motor6D joints for
-- limited BallSocketConstraints for `seconds`, then restores them. Rigs
-- without Motor6Ds (the capsule fallback passengers) get PlatformStand
-- instead. Re-ragdolling refreshes the timer via an epoch attribute.
--
-- Works on both NPC rigs and player characters: Motor6D.Enabled and the
-- constraints replicate, so server-side calls ragdoll everyone visibly.

local RagdollUtil = {}

function RagdollUtil.ragdoll(character: Model, seconds: number)
	local humanoid = character:FindFirstChildOfClass("Humanoid")
	if not humanoid or humanoid.Health <= 0 then
		return
	end

	local epoch = 1
	local previous = character:GetAttribute("VE_RagdollEpoch")
	if typeof(previous) == "number" then
		epoch = previous + 1
	end
	character:SetAttribute("VE_RagdollEpoch", epoch)

	local motors: { Motor6D } = {}
	for _, descendant in character:GetDescendants() do
		if
			descendant:IsA("Motor6D")
			and descendant.Name ~= "Root"
			and descendant.Name ~= "RootJoint"
		then
			table.insert(motors, descendant)
		end
	end

	if #motors == 0 then
		humanoid.PlatformStand = true
		task.delay(seconds, function()
			if character:GetAttribute("VE_RagdollEpoch") == epoch and humanoid.Health > 0 then
				humanoid.PlatformStand = false
			end
		end)
		return
	end

	humanoid.AutoRotate = false
	humanoid:ChangeState(Enum.HumanoidStateType.Physics)

	local created: { Instance } = {}
	for _, motor in motors do
		local part0, part1 = motor.Part0, motor.Part1
		if part0 and part1 then
			local attachment0 = Instance.new("Attachment")
			attachment0.CFrame = motor.C0
			attachment0.Parent = part0
			local attachment1 = Instance.new("Attachment")
			attachment1.CFrame = motor.C1
			attachment1.Parent = part1
			local socket = Instance.new("BallSocketConstraint")
			socket.Attachment0 = attachment0
			socket.Attachment1 = attachment1
			socket.LimitsEnabled = true
			socket.TwistLimitsEnabled = true
			socket.UpperAngle = 60
			socket.Parent = part0
			table.insert(created, attachment0)
			table.insert(created, attachment1)
			table.insert(created, socket)
			motor.Enabled = false
		end
	end

	task.delay(seconds, function()
		if character:GetAttribute("VE_RagdollEpoch") ~= epoch then
			return
		end
		for _, instance in created do
			instance:Destroy()
		end
		for _, motor in motors do
			if motor.Parent then
				motor.Enabled = true
			end
		end
		if humanoid.Health > 0 then
			humanoid.AutoRotate = true
			humanoid:ChangeState(Enum.HumanoidStateType.GettingUp)
		end
	end)
end

return RagdollUtil
