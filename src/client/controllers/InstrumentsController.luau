--!strict
-- Renders the cockpit instrument cluster as a client-side SurfaceGui adorned
-- to the VE_CockpitDash part (back face, toward the pilot seat) and refreshes
-- it from the plane model's attributes: Airspeed, Altitude, VerticalSpeed,
-- HeadingDeg, EngineThrottle (+AutopilotOn), FuelKg.

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local FlightConfig = require(Shared:WaitForChild("FlightConfig"))

local InstrumentsController = {}
InstrumentsController.name = "InstrumentsController"

local localPlayer = Players.LocalPlayer

local COLOR_TITLE = Color3.fromRGB(160, 168, 176)
local COLOR_VALUE = Color3.fromRGB(110, 255, 150)
local COLOR_WARNING = Color3.fromRGB(255, 90, 80)
local COLOR_BACKGROUND = Color3.fromRGB(12, 14, 16)

local function makeCell(parent: Instance, order: number, title: string): TextLabel
	local cell = Instance.new("Frame")
	cell.Name = title
	cell.LayoutOrder = order
	cell.BackgroundColor3 = COLOR_BACKGROUND
	cell.BackgroundTransparency = 0.15
	cell.BorderSizePixel = 0

	local titleLabel = Instance.new("TextLabel")
	titleLabel.Name = "Title"
	titleLabel.Size = UDim2.fromScale(1, 0.35)
	titleLabel.BackgroundTransparency = 1
	titleLabel.Font = Enum.Font.Code
	titleLabel.Text = title
	titleLabel.TextColor3 = COLOR_TITLE
	titleLabel.TextScaled = true
	titleLabel.Parent = cell

	local valueLabel = Instance.new("TextLabel")
	valueLabel.Name = "Value"
	valueLabel.Position = UDim2.fromScale(0, 0.35)
	valueLabel.Size = UDim2.fromScale(1, 0.65)
	valueLabel.BackgroundTransparency = 1
	valueLabel.Font = Enum.Font.Code
	valueLabel.Text = "--"
	valueLabel.TextColor3 = COLOR_VALUE
	valueLabel.TextScaled = true
	valueLabel.Parent = cell

	cell.Parent = parent
	return valueLabel
end

function InstrumentsController.start()
	local plane = Refs.waitFor(Refs.Tags.Plane) :: Model
	local dash = Refs.waitFor(Refs.Tags.CockpitDash) :: BasePart

	local gui = Instance.new("SurfaceGui")
	gui.Name = "VE_Instruments"
	gui.Adornee = dash
	gui.Face = Enum.NormalId.Back
	gui.LightInfluence = 0
	gui.PixelsPerStud = 50

	local layout = Instance.new("UIGridLayout")
	layout.CellSize = UDim2.fromScale(1 / 3, 0.5)
	layout.CellPadding = UDim2.fromOffset(0, 0)
	layout.SortOrder = Enum.SortOrder.LayoutOrder
	layout.Parent = gui

	local speedValue = makeCell(gui, 1, "SPD")
	local altitudeValue = makeCell(gui, 2, "ALT")
	local verticalValue = makeCell(gui, 3, "V/S")
	local headingValue = makeCell(gui, 4, "HDG")
	local throttleValue = makeCell(gui, 5, "THR")
	local fuelValue = makeCell(gui, 6, "FUEL")

	gui.Parent = localPlayer:WaitForChild("PlayerGui")

	local function readNumber(name: string): number
		local value = plane:GetAttribute(name)
		return if typeof(value) == "number" then value else 0
	end

	while true do
		speedValue.Text = ("%d"):format(readNumber("Airspeed"))
		altitudeValue.Text = ("%d"):format(readNumber("Altitude"))
		verticalValue.Text = ("%+d"):format(readNumber("VerticalSpeed"))
		headingValue.Text = ("%03d"):format(readNumber("HeadingDeg"))

		local throttlePercent = math.round(readNumber("EngineThrottle") * 100)
		local autopilotOn = plane:GetAttribute("AutopilotOn") == true
		throttleValue.Text = if autopilotOn
			then ("%d%% AP"):format(throttlePercent)
			else ("%d%%"):format(throttlePercent)

		local fuel = readNumber("FuelKg")
		fuelValue.Text = ("%d kg"):format(fuel)
		fuelValue.TextColor3 = if fuel
				< FlightConfig.FuelCapacityKg * FlightConfig.LowFuelFraction
			then COLOR_WARNING
			else COLOR_VALUE

		task.wait(FlightConfig.InstrumentsUpdateSeconds)
	end
end

return InstrumentsController
