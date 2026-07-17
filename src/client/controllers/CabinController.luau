--!strict
-- Cabin-side client feedback: a top-center toast feed driven by the "Cabin"
-- NetBus channel, and a camera shake while turbulence is active.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")
local TweenService = game:GetService("TweenService")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local CabinConfig = require(Shared:WaitForChild("CabinConfig"))
local NetBus = require(Shared:WaitForChild("NetBus"))

local CabinController = {}
CabinController.name = "CabinController"

local localPlayer = Players.LocalPlayer
local MAX_TOASTS = 5

function CabinController.start()
	local bus = NetBus.client("Cabin")

	local screenGui = Instance.new("ScreenGui")
	screenGui.Name = "VE_Cabin"
	screenGui.ResetOnSpawn = false
	local list = Instance.new("Frame")
	list.Name = "Toasts"
	list.AnchorPoint = Vector2.new(0.5, 0)
	list.Position = UDim2.new(0.5, 0, 0, 12)
	list.Size = UDim2.new(0, 460, 0, 200)
	list.BackgroundTransparency = 1
	local layout = Instance.new("UIListLayout")
	layout.HorizontalAlignment = Enum.HorizontalAlignment.Center
	layout.SortOrder = Enum.SortOrder.LayoutOrder
	layout.Padding = UDim.new(0, 4)
	layout.Parent = list
	list.Parent = screenGui
	screenGui.Parent = localPlayer:WaitForChild("PlayerGui")

	local toastOrder = 0
	bus.on("Toast", function(data)
		local text = data and data.text
		if typeof(text) ~= "string" then
			return
		end
		toastOrder += 1
		local label = Instance.new("TextLabel")
		label.LayoutOrder = toastOrder
		label.Size = UDim2.new(1, 0, 0, 28)
		label.BackgroundColor3 = Color3.fromRGB(12, 14, 16)
		label.BackgroundTransparency = 0.35
		label.Font = Enum.Font.GothamMedium
		label.TextSize = 17
		label.TextColor3 = Color3.fromRGB(240, 240, 240)
		label.TextTruncate = Enum.TextTruncate.AtEnd
		label.Text = text
		label.Parent = list

		local children = 0
		for _, child in list:GetChildren() do
			if child:IsA("TextLabel") then
				children += 1
			end
		end
		if children > MAX_TOASTS then
			local oldest: TextLabel? = nil
			for _, child in list:GetChildren() do
				if
					child:IsA("TextLabel")
					and (not oldest or child.LayoutOrder < oldest.LayoutOrder)
				then
					oldest = child
				end
			end
			if oldest then
				oldest:Destroy()
			end
		end

		task.delay(CabinConfig.ToastSeconds, function()
			if label.Parent then
				local tween = TweenService:Create(
					label,
					TweenInfo.new(0.4),
					{ TextTransparency = 1, BackgroundTransparency = 1 }
				)
				tween:Play()
				tween.Completed:Once(function()
					label:Destroy()
				end)
			end
		end)
	end)

	-- Turbulence camera shake; the token cancels stale shakes on overlap.
	local shakeToken = 0
	bus.on("Turbulence", function(data)
		local duration = data and data.d
		if typeof(duration) ~= "number" or duration <= 0 or duration > 15 then
			return
		end
		shakeToken += 1
		local token = shakeToken
		local rng = Random.new()
		task.spawn(function()
			local deadline = os.clock() + duration
			while os.clock() < deadline and token == shakeToken do
				local character = localPlayer.Character
				local humanoid = character and character:FindFirstChildOfClass("Humanoid")
				if humanoid then
					humanoid.CameraOffset = Vector3.new(
						rng:NextNumber(-CabinConfig.ShakeMagnitude, CabinConfig.ShakeMagnitude),
						rng:NextNumber(-CabinConfig.ShakeMagnitude, CabinConfig.ShakeMagnitude),
						0
					)
				end
				RunService.RenderStepped:Wait()
			end
			if token == shakeToken then
				local character = localPlayer.Character
				local humanoid = character and character:FindFirstChildOfClass("Humanoid")
				if humanoid then
					humanoid.CameraOffset = Vector3.zero
				end
			end
		end)
	end)
end

return CabinController
