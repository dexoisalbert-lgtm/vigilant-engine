--!strict
-- Single tuning surface for the cabin: passengers, demands, service items,
-- turbulence, and scoring. Same pattern as FlightConfig.

local CabinConfig = {
	-- Passengers.
	PassengerCount = 10, -- seats not filled stay free for players to buckle into
	PassengerWalkSpeed = 8,
	PassengerRespawnSeconds = 12,
	PassengerNames = {
		"Karen",
		"Bob",
		"Tina",
		"Greg",
		"Paula",
		"Stu",
		"Mabel",
		"Dex",
		"Rhonda",
		"Chip",
		"Lois",
		"Ernie",
	},
	SkinTones = {
		Color3.fromRGB(250, 214, 180),
		Color3.fromRGB(234, 184, 146),
		Color3.fromRGB(198, 133, 102),
		Color3.fromRGB(146, 92, 62),
		Color3.fromRGB(108, 70, 52),
	},
	ShirtColors = {
		Color3.fromRGB(196, 40, 28),
		Color3.fromRGB(13, 105, 172),
		Color3.fromRGB(245, 205, 48),
		Color3.fromRGB(75, 151, 75),
		Color3.fromRGB(160, 95, 53),
		Color3.fromRGB(107, 50, 124),
	},
	PantsColors = {
		Color3.fromRGB(40, 40, 48),
		Color3.fromRGB(82, 84, 92),
		Color3.fromRGB(27, 42, 53),
		Color3.fromRGB(116, 134, 157),
	},

	-- Demands. A passenger waits, wants an item, loses patience, then rages.
	Items = {
		{ id = "Coffee", emoji = "\u{2615}" },
		{ id = "Soda", emoji = "\u{1F964}" },
		{ id = "Burger", emoji = "\u{1F354}" },
	},
	FirstDemandMinSeconds = 8,
	FirstDemandMaxSeconds = 25,
	DemandMinSeconds = 15,
	DemandMaxSeconds = 40,
	PatienceSeconds = 45,
	ServeScore = 10,

	-- Enraged passengers wander the cabin and shove nearby players.
	EnrageSeconds = 20,
	EnrageShoveRadius = 5,
	EnrageShoveSpeed = 20,
	EnrageShoveCooldownSeconds = 2,
	ReturnTimeoutSeconds = 8,
	RagdollSeconds = 2.5,

	-- Turbulence: periodic while airborne; unbuckled occupants get tossed.
	TurbulenceMinIntervalSeconds = 35,
	TurbulenceMaxIntervalSeconds = 75,
	TurbulenceDurationSeconds = 2.5,
	TurbulenceMinAirspeed = 60,
	TurbulenceTossSpeed = 22, -- horizontal, studs/s
	TurbulenceUpSpeed = 12,

	-- Coffee: sip for a buff; sip in freefall for the Coffee Break Plus.
	CoffeeSips = 3,
	CoffeeSipScore = 2,
	CoffeeSipCooldownSeconds = 1.5,
	CaffeinatedWalkSpeed = 22,
	CaffeinatedSeconds = 6,
	CoffeeBreakPlusScore = 25,
	CoffeeBreakPlusMinAirspeed = 50,

	-- Galley.
	MaxCarriedItems = 3,

	-- Client feedback.
	ToastSeconds = 3,
	ShakeMagnitude = 0.35,
}

return CabinConfig
