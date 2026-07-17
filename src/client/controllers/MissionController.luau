--!strict
-- Mission HUD (Phase 6/7): top-right banner showing the current mission,
-- phase + countdown, objective text, and the contraband tally when relevant.
-- Reads attributes from ReplicatedStorage.VE_MissionState on a light poll.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local MissionController = {}
MissionController.name = "MissionController"

local localPlayer = Players.LocalPlayer

local COLOR_BACKGROUND = Color3.fromRGB(12, 14, 16)
local COLOR_TITLE = Color3.fromRGB(255, 220, 90)
local COLOR_TEXT = Color3.fromRGB(235, 235, 235)
local COLOR_DIM = Color3.fromRGB(160, 168, 176)

local function makeLabel(parent: Instance, order: number, height: number): TextLabel
	local label = Instance.new("TextLabel")
	label.LayoutOrder = order
	label.Size = UDim2.new(1, 0, 0, height)
	label.BackgroundTransparency = 1
	label.Font = Enum.Font.GothamMedium
	label.TextColor3 = COLOR_TEXT
	label.TextSize = 14
	label.TextXAlignment = Enum.TextXAlignment.Right
	label.TextWrapped = true
	label.Text = ""
	label.Parent = parent
	return label
end

function MissionController.start()
	local state = ReplicatedStorage:WaitForChild("VE_MissionState")

	local screenGui = Instance.new("ScreenGui")
	screenGui.Name = "VE_Mission"
	screenGui.ResetOnSpawn = false

	local panel = Instance.new("Frame")
	panel.Name = "Banner"
	panel.AnchorPoint = Vector2.new(1, 0)
	panel.Position = UDim2.new(1, -12, 0, 12)
	panel.Size = UDim2.new(0, 260, 0, 96)
	panel.BackgroundColor3 = COLOR_BACKGROUND
	panel.BackgroundTransparency = 0.35
	panel.BorderSizePixel = 0
	local padding = Instance.new("UIPadding")
	padding.PaddingLeft = UDim.new(0, 8)
	padding.PaddingRight = UDim.new(0, 8)
	padding.PaddingTop = UDim.new(0, 6)
	padding.Parent = panel
	local layout = Instance.new("UIListLayout")
	layout.SortOrder = Enum.SortOrder.LayoutOrder
	layout.Padding = UDim.new(0, 2)
	layout.Parent = panel

	local titleLabel = makeLabel(panel, 1, 20)
	titleLabel.Font = Enum.Font.GothamBold
	titleLabel.TextSize = 17
	titleLabel.TextColor3 = COLOR_TITLE
	local phaseLabel = makeLabel(panel, 2, 16)
	phaseLabel.TextColor3 = COLOR_DIM
	local objectiveLabel = makeLabel(panel, 3, 40)
	local contrabandLabel = makeLabel(panel, 4, 16)
	contrabandLabel.TextColor3 = COLOR_TITLE

	panel.Parent = screenGui
	screenGui.Parent = localPlayer:WaitForChild("PlayerGui")

	local function readString(name: string): string
		local value = state:GetAttribute(name)
		return if typeof(value) == "string" then value else ""
	end

	local function readNumber(name: string): number
		local value = state:GetAttribute(name)
		return if typeof(value) == "number" then value else 0
	end

	while true do
		titleLabel.Text = readString("MissionName")
		local seconds = readNumber("SecondsRemaining")
		phaseLabel.Text = ("%s  %d:%02d"):format(
			readString("Phase"),
			math.floor(seconds / 60),
			seconds % 60
		)
		objectiveLabel.Text = readString("Objective")
		local total = readNumber("ContrabandTotal")
		contrabandLabel.Text = if total > 0
			then ("\u{1F4E6} %d/%d delivered"):format(readNumber("ContrabandDelivered"), total)
			else ""
		task.wait(0.5)
	end
end

return MissionController
