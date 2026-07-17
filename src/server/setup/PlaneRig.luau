--!strict
-- Programmatically constructs the Phase 1 airliner: hollow cabin, cockpit with
-- VehicleSeat + dashboard, wings, engines, tail, fixed gear, boarding step,
-- and the physics constraints the FlightController drives (thrust/lift/drag
-- VectorForces + AlignOrientation). Applies all Refs tags and initial
-- attributes, so everything downstream binds by tag, never by hierarchy.
--
-- Local frame: the root part is the cabin floor, centered at the model origin.
-- Forward is -Z, right is +X, up is +Y.

local CollectionService = game:GetService("CollectionService")
local ReplicatedStorage = game:GetService("ReplicatedStorage")

local Shared = ReplicatedStorage:WaitForChild("VEShared")
local Refs = require(Shared:WaitForChild("Refs"))
local FlightConfig = require(Shared:WaitForChild("FlightConfig"))
local CargoConfig = require(Shared:WaitForChild("CargoConfig"))

local PlaneRig = {}

local COLOR_HULL = Color3.fromRGB(233, 236, 236)
local COLOR_FLOOR = Color3.fromRGB(99, 95, 98)
local COLOR_ACCENT = Color3.fromRGB(196, 40, 28)
local COLOR_DARK = Color3.fromRGB(27, 42, 53)
local COLOR_GLASS = Color3.fromRGB(163, 200, 220)
local COLOR_SEAT = Color3.fromRGB(52, 87, 132)

type PartOptions = {
	color: Color3?,
	material: Enum.Material?,
	transparency: number?,
	canCollide: boolean?,
	shape: Enum.PartType?,
	repairZone: string?, -- "Hull" | "Wing" | "Engine": tags the part VE_Repairable
}

local function makePart(name: string, size: Vector3, localCf: CFrame, options: PartOptions?): Part
	local opts: PartOptions = options or {}
	local part = Instance.new("Part")
	part.Name = name
	part.Size = size
	part.CFrame = localCf
	part.Color = opts.color or COLOR_HULL
	part.Material = opts.material or Enum.Material.SmoothPlastic
	part.Transparency = opts.transparency or 0
	part.CanCollide = if opts.canCollide == nil then true else opts.canCollide
	if opts.shape then
		part.Shape = opts.shape
	end
	part.Anchored = false
	part.Massless = true
	part.TopSurface = Enum.SurfaceType.Smooth
	part.BottomSurface = Enum.SurfaceType.Smooth
	return part
end

local function weldTo(root: BasePart, part: BasePart)
	local weld = Instance.new("WeldConstraint")
	weld.Part0 = root
	weld.Part1 = part
	weld.Parent = part
end

function PlaneRig.build(spawnCf: CFrame): Model
	local plane = Instance.new("Model")
	plane.Name = "DoomedAirliner"

	-- Root: the cabin floor. The only massy part, so the center of mass stays
	-- at the model origin and the force attachment below acts through it.
	local root = makePart("Fuselage", Vector3.new(10, 1, 46), spawnCf, { color = COLOR_FLOOR })
	root.Massless = false
	root.Parent = plane
	plane.PrimaryPart = root

	local function addPart(name: string, size: Vector3, offset: CFrame, options: PartOptions?): Part
		local part = makePart(name, size, spawnCf * offset, options)
		if options and options.repairZone then
			part:SetAttribute("DamageZone", options.repairZone)
			CollectionService:AddTag(part, Refs.Tags.Repairable)
		end
		part.Parent = plane
		weldTo(root, part)
		return part
	end

	-- Cabin shell (interior space y 0.5..7.5; door gap in the left wall).
	-- Everything here is a repairable hull panel.
	addPart("WallRight", Vector3.new(1, 7, 46), CFrame.new(4.5, 4, 0), { repairZone = "Hull" })
	addPart(
		"WallLeftFront",
		Vector3.new(1, 7, 18),
		CFrame.new(-4.5, 4, -14),
		{ repairZone = "Hull" }
	)
	addPart("WallLeftRear", Vector3.new(1, 7, 22), CFrame.new(-4.5, 4, 12), { repairZone = "Hull" })
	addPart("Ceiling", Vector3.new(10, 1, 46), CFrame.new(0, 8, 0), { repairZone = "Hull" })
	addPart("NoseLower", Vector3.new(10, 3, 1), CFrame.new(0, 2, -22.5), { repairZone = "Hull" })

	-- Bulkhead between cabin and cargo bay, with a center doorway.
	addPart("BulkheadLeft", Vector3.new(3.5, 7, 1), CFrame.new(-3.25, 4, 22.5), {
		repairZone = "Hull",
	})
	addPart("BulkheadRight", Vector3.new(3.5, 7, 1), CFrame.new(3.25, 4, 22.5), {
		repairZone = "Hull",
	})

	-- Cargo bay (z 23..37) aft of the bulkhead, ending at the ramp opening.
	local cargoFloor = addPart("CargoFloor", Vector3.new(10, 1, 14), CFrame.new(0, 0, 30), {
		color = COLOR_FLOOR,
	})
	CollectionService:AddTag(cargoFloor, Refs.Tags.CargoHold)
	addPart("CargoWallLeft", Vector3.new(1, 7, 14), CFrame.new(-4.5, 4, 30), {
		repairZone = "Hull",
	})
	addPart("CargoWallRight", Vector3.new(1, 7, 14), CFrame.new(4.5, 4, 30), {
		repairZone = "Hull",
	})
	addPart("CargoCeiling", Vector3.new(10, 1, 14), CFrame.new(0, 8, 30), { repairZone = "Hull" })
	addPart("RackPadLeft", Vector3.new(3, 0.2, 12), CFrame.new(-2.2, 0.6, 30), {
		color = COLOR_DARK,
	})
	addPart("RackPadRight", Vector3.new(3, 0.2, 12), CFrame.new(2.2, 0.6, 30), {
		color = COLOR_DARK,
	})

	-- Smuggler compartment: a hidden alcove panel at the bay's front-left
	-- corner. MissionService wires the toggle prompt and spawns contraband
	-- behind it during Smuggle missions.
	local smugglerPanel =
		addPart("SmugglerPanel", Vector3.new(2.5, 3, 0.4), CFrame.new(-3, 2, 25), {
			color = COLOR_FLOOR,
			material = Enum.Material.DiamondPlate,
		})
	CollectionService:AddTag(smugglerPanel, Refs.Tags.SmugglerHold)
	local panelPrompt = Instance.new("ProximityPrompt")
	panelPrompt.Name = "PanelPrompt"
	panelPrompt.ActionText = "Open Panel"
	panelPrompt.ObjectText = "Loose Panel"
	panelPrompt.HoldDuration = 0.6
	panelPrompt.MaxActivationDistance = 6
	panelPrompt.RequiresLineOfSight = false
	panelPrompt.Parent = smugglerPanel

	local holdLight = addPart("CargoLight", Vector3.new(1.5, 0.2, 1.5), CFrame.new(0, 7.6, 30), {
		color = Color3.fromRGB(248, 248, 240),
		material = Enum.Material.Neon,
	})
	local holdPointLight = Instance.new("PointLight")
	holdPointLight.Brightness = 0.8
	holdPointLight.Range = 14
	holdPointLight.Color = Color3.fromRGB(255, 244, 224)
	holdPointLight.Parent = holdLight

	-- Rear ramp: welded in either the closed (vertical) or open (slide) pose;
	-- CargoService re-welds it between the two on prompt toggle.
	local ramp = addPart(
		"CargoRamp",
		Vector3.new(10, 0.8, 8),
		CFrame.new(CargoConfig.RampClosedPosition)
			* CFrame.Angles(math.rad(CargoConfig.RampClosedPitchDeg), 0, 0),
		{ color = COLOR_FLOOR, material = Enum.Material.DiamondPlate }
	)
	CollectionService:AddTag(ramp, Refs.Tags.CargoRamp)
	local rampPrompt = Instance.new("ProximityPrompt")
	rampPrompt.Name = "RampPrompt"
	rampPrompt.ActionText = "Open Ramp"
	rampPrompt.ObjectText = "Cargo Ramp"
	rampPrompt.HoldDuration = 0.6
	rampPrompt.MaxActivationDistance = 10
	rampPrompt.RequiresLineOfSight = false
	rampPrompt.Parent = ramp

	local windshield = addPart("Windshield", Vector3.new(10, 4, 1), CFrame.new(0, 5.5, -22.5), {
		color = COLOR_GLASS,
		material = Enum.Material.Glass,
		transparency = 0.45,
		repairZone = "Hull",
	})
	CollectionService:AddTag(windshield, Refs.Tags.CockpitWindow)

	-- Cockpit interior.
	local dash = addPart("Dashboard", Vector3.new(8, 2, 1), CFrame.new(0, 3.5, -21.5), {
		color = COLOR_DARK,
	})
	CollectionService:AddTag(dash, Refs.Tags.CockpitDash)

	local autopilotPrompt = Instance.new("ProximityPrompt")
	autopilotPrompt.Name = "AutopilotPrompt"
	autopilotPrompt.ActionText = "Toggle Autopilot"
	autopilotPrompt.ObjectText = "Autopilot"
	autopilotPrompt.HoldDuration = 0.4
	autopilotPrompt.MaxActivationDistance = 8
	autopilotPrompt.RequiresLineOfSight = false
	autopilotPrompt.Parent = dash

	local seat = Instance.new("VehicleSeat")
	seat.Name = "PilotSeat"
	seat.Size = Vector3.new(2, 0.8, 2)
	seat.CFrame = spawnCf * CFrame.new(0, 0.9, -19)
	seat.Color = COLOR_DARK
	seat.Material = Enum.Material.Fabric
	seat.Anchored = false
	seat.Massless = true
	seat.HeadsUpDisplay = false
	seat.Parent = plane
	weldTo(root, seat)
	CollectionService:AddTag(seat, Refs.Tags.PilotSeat)

	-- Wings: left/right panels stopping at the fuselage walls so they don't
	-- cut through the cabin interior. Walkable for wing-walking later.
	for side = -1, 1, 2 do
		local wing = addPart(
			if side < 0 then "WingLeft" else "WingRight",
			Vector3.new(11, 0.8, 8),
			CFrame.new(9.5 * side, 2, 2),
			{ color = COLOR_HULL, repairZone = "Wing" }
		)
		CollectionService:AddTag(wing, Refs.Tags.Wing)
	end

	-- Gatling turrets on each wing top: base, fixed barrel, and a gunner
	-- seat. CombatService does the hitreg; the barrel is cosmetic.
	for side = -1, 1, 2 do
		local suffix = if side < 0 then "Left" else "Right"
		addPart(
			"GatlingBase" .. suffix,
			Vector3.new(1.6, 0.8, 1.6),
			CFrame.new(9.5 * side, 2.8, 3),
			{
				color = COLOR_DARK,
				material = Enum.Material.Metal,
			}
		)
		addPart(
			"GatlingBarrel" .. suffix,
			Vector3.new(0.5, 0.5, 4),
			CFrame.new(9.5 * side, 3.6, 0.8),
			{ color = COLOR_DARK, material = Enum.Material.Metal }
		)
		local gunSeat = Instance.new("Seat")
		gunSeat.Name = "GatlingSeat" .. suffix
		gunSeat.Size = Vector3.new(2, 0.5, 2)
		gunSeat.CFrame = spawnCf * CFrame.new(9.5 * side, 2.65, 5)
		gunSeat.Color = COLOR_DARK
		gunSeat.Material = Enum.Material.Fabric
		gunSeat.Anchored = false
		gunSeat.Massless = true
		gunSeat.Parent = plane
		weldTo(root, gunSeat)
		CollectionService:AddTag(gunSeat, Refs.Tags.GatlingSeat)
	end

	-- Engines: nacelles under each wing.
	for side = -1, 1, 2 do
		local engine = addPart(
			if side < 0 then "EngineLeft" else "EngineRight",
			Vector3.new(5, 2.4, 2.4),
			CFrame.new(8 * side, 0.8, 2) * CFrame.Angles(0, math.rad(90), 0),
			{
				color = COLOR_DARK,
				material = Enum.Material.Metal,
				shape = Enum.PartType.Cylinder,
				repairZone = "Engine",
			}
		)
		engine:SetAttribute("EngineHealth", 100)
		CollectionService:AddTag(engine, Refs.Tags.Engine)

		-- Thrust wash particles; FlightController drives Rate/Enabled from the
		-- live throttle.
		local thrustEmitter = Instance.new("ParticleEmitter")
		thrustEmitter.Name = "VE_ThrustEmitter"
		thrustEmitter.Enabled = false
		thrustEmitter.Rate = 0
		thrustEmitter.Lifetime = NumberRange.new(0.2, 0.35)
		thrustEmitter.Speed = NumberRange.new(18, 26)
		thrustEmitter.Size = NumberSequence.new(0.6)
		thrustEmitter.Color =
			ColorSequence.new(Color3.fromRGB(255, 170, 80), Color3.fromRGB(120, 120, 130))
		thrustEmitter.Transparency = NumberSequence.new(0.4)
		thrustEmitter.Parent = engine
	end

	-- Cabin seating: two columns with a center aisle. The left seat is omitted
	-- where a row would block the boarding door (wall gap z -5..1).
	local seatRows = { -12, -8, -4, 0, 4, 8, 12 }
	for _, rowZ in seatRows do
		for side = -1, 1, 2 do
			if side < 0 and (rowZ == -4 or rowZ == 0) then
				continue
			end
			local cabinSeat = Instance.new("Seat")
			cabinSeat.Name = ("CabinSeat_%s%d"):format(if side < 0 then "L" else "R", rowZ)
			cabinSeat.Size = Vector3.new(2, 0.8, 2)
			cabinSeat.CFrame = spawnCf * CFrame.new(2.5 * side, 0.9, rowZ)
			cabinSeat.Color = COLOR_SEAT
			cabinSeat.Material = Enum.Material.Fabric
			cabinSeat.Anchored = false
			cabinSeat.Massless = true
			cabinSeat:SetAttribute("Buckled", false)
			cabinSeat.Parent = plane
			weldTo(root, cabinSeat)
			CollectionService:AddTag(cabinSeat, Refs.Tags.CabinSeat)

			local bucklePrompt = Instance.new("ProximityPrompt")
			bucklePrompt.Name = "BucklePrompt"
			bucklePrompt.ActionText = "Buckle Seatbelt"
			bucklePrompt.ObjectText = "Seatbelt"
			bucklePrompt.HoldDuration = 0.3
			bucklePrompt.MaxActivationDistance = 7
			bucklePrompt.RequiresLineOfSight = false
			bucklePrompt.Parent = cabinSeat
		end
	end

	-- Galley: rear counter with item dispensers the crew grab tools from.
	addPart("GalleyCounter", Vector3.new(7, 2.5, 1.2), CFrame.new(0, 1.75, 20.5), {
		color = COLOR_DARK,
		material = Enum.Material.Metal,
	})
	local dispenserItems = {
		{ item = "Coffee", color = Color3.fromRGB(122, 84, 60) },
		{ item = "Soda", color = Color3.fromRGB(30, 120, 220) },
		{ item = "Burger", color = Color3.fromRGB(214, 133, 46) },
		{ item = "Tray", color = Color3.fromRGB(200, 200, 205) },
	}
	for index, entry in dispenserItems do
		local dispenser = addPart(
			"Dispenser" .. entry.item,
			Vector3.new(1.2, 1.2, 1),
			CFrame.new(-2.7 + (index - 1) * 1.8, 3.6, 20.6),
			{ color = entry.color, material = Enum.Material.Metal }
		)
		dispenser:SetAttribute("Item", entry.item)
		CollectionService:AddTag(dispenser, Refs.Tags.Dispenser)

		local dispensePrompt = Instance.new("ProximityPrompt")
		dispensePrompt.Name = "DispensePrompt"
		dispensePrompt.ActionText = "Take " .. entry.item
		dispensePrompt.ObjectText = "Galley"
		dispensePrompt.HoldDuration = 0.25
		dispensePrompt.MaxActivationDistance = 6
		dispensePrompt.RequiresLineOfSight = false
		dispensePrompt.Parent = dispenser
	end

	-- Repair locker: cockpit-side wall, dispensing the maintenance tools.
	addPart("RepairLocker", Vector3.new(1, 2.5, 4), CFrame.new(-3.4, 1.75, -18.5), {
		color = COLOR_ACCENT,
		material = Enum.Material.Metal,
	})
	local repairItems = {
		{ item = "Blowtorch", label = "Blowtorch", color = Color3.fromRGB(60, 90, 140) },
		{ item = "Hammer", label = "Hammer", color = Color3.fromRGB(120, 110, 100) },
		{
			item = "FireExtinguisher",
			label = "Fire Extinguisher",
			color = Color3.fromRGB(200, 40, 30),
		},
		{ item = "IceAxe", label = "Ice Axe", color = Color3.fromRGB(160, 200, 210) },
	}
	for index, entry in repairItems do
		local row = math.floor((index - 1) / 2)
		local column = (index - 1) % 2
		local dispenser = addPart(
			"Dispenser" .. entry.item,
			Vector3.new(0.7, 0.9, 0.9),
			CFrame.new(-2.6, 1.3 + row * 1.2, -19.5 + column * 2),
			{ color = entry.color, material = Enum.Material.Metal }
		)
		dispenser:SetAttribute("Item", entry.item)
		CollectionService:AddTag(dispenser, Refs.Tags.Dispenser)

		local dispensePrompt = Instance.new("ProximityPrompt")
		dispensePrompt.Name = "DispensePrompt"
		dispensePrompt.ActionText = "Take " .. entry.label
		dispensePrompt.ObjectText = "Repair Locker"
		dispensePrompt.HoldDuration = 0.25
		dispensePrompt.MaxActivationDistance = 6
		dispensePrompt.RequiresLineOfSight = false
		dispensePrompt.Parent = dispenser
	end

	-- Cabin ceiling lights so the enclosed interior isn't pitch black.
	for _, lightZ in { -10, 2, 14 } do
		local fixture =
			addPart("CabinLight", Vector3.new(1.5, 0.2, 1.5), CFrame.new(0, 7.6, lightZ), {
				color = Color3.fromRGB(248, 248, 240),
				material = Enum.Material.Neon,
			})
		local light = Instance.new("PointLight")
		light.Brightness = 0.8
		light.Range = 14
		light.Color = Color3.fromRGB(255, 244, 224)
		light.Parent = fixture
	end

	-- Tail (atop the cargo bay, at the new rear of the plane).
	addPart("VerticalStabilizer", Vector3.new(0.8, 5, 7), CFrame.new(0, 11, 33), {
		color = COLOR_ACCENT,
	})
	addPart("HorizontalStabilizer", Vector3.new(14, 0.8, 5), CFrame.new(0, 8.9, 34), {
		color = COLOR_HULL,
	})

	-- Fixed tricycle gear (Phase 1: always down). Low-friction spheres.
	local wheelPhysics = PhysicalProperties.new(0.7, 0.05, 0.2)
	local wheelOffsets = {
		Vector3.new(0, -1, -18),
		Vector3.new(-3.5, -1, 6),
		Vector3.new(3.5, -1, 6),
	}
	for index, offset in wheelOffsets do
		local wheel = addPart("Wheel" .. index, Vector3.new(2, 2, 2), CFrame.new(offset), {
			color = COLOR_DARK,
			material = Enum.Material.Rubber,
			shape = Enum.PartType.Ball,
		})
		wheel.CustomPhysicalProperties = wheelPhysics
	end

	-- Boarding step outside the door gap.
	addPart("BoardingStep", Vector3.new(3, 1, 4), CFrame.new(-6, -1.5, -2), {
		color = COLOR_FLOOR,
	})

	-- Physics rig: forces + orientation controller, all driven by the
	-- FlightController each Heartbeat.
	local attachment = Instance.new("Attachment")
	attachment.Name = "VE_RootAttachment"
	attachment.Parent = root

	local function makeForce(name: string, relativeTo: Enum.ActuatorRelativeTo): VectorForce
		local force = Instance.new("VectorForce")
		force.Name = name
		force.Attachment0 = attachment
		force.RelativeTo = relativeTo
		force.ApplyAtCenterOfMass = true
		force.Force = Vector3.zero
		force.Parent = root
		return force
	end

	makeForce("VE_ThrustForce", Enum.ActuatorRelativeTo.Attachment0)
	makeForce("VE_LiftForce", Enum.ActuatorRelativeTo.Attachment0)
	makeForce("VE_DragForce", Enum.ActuatorRelativeTo.World)

	local align = Instance.new("AlignOrientation")
	align.Name = "VE_Align"
	align.Mode = Enum.OrientationAlignmentMode.OneAttachment
	align.Attachment0 = attachment
	align.RigidityEnabled = false
	align.MaxTorque = 1e9
	align.Responsiveness = 25
	align.CFrame = spawnCf.Rotation
	align.Parent = root

	-- Plane-wide state attributes (see docs/GAME_PLAN.md, Data Model).
	plane:SetAttribute("FuelKg", FlightConfig.FuelCapacityKg)
	plane:SetAttribute("EngineThrottle", 0)
	plane:SetAttribute("AutopilotOn", false)
	plane:SetAttribute("GearDown", true)
	plane:SetAttribute("FlapsDeg", 0)
	plane:SetAttribute("HullIntegrity", 100)

	CollectionService:AddTag(plane, Refs.Tags.Plane)
	CollectionService:AddTag(root, Refs.Tags.PlanePrimary)

	plane.Parent = workspace

	-- Server keeps physics authority; pilot input arrives over the NetBus.
	pcall(function()
		root:SetNetworkOwner(nil)
	end)

	return plane
end

return PlaneRig
