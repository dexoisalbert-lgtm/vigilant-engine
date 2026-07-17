--!strict
-- Per-player SESSION scoring via the standard leaderstats folder. Other
-- services call ScoreService.award(); the leaderstats IntValue is created
-- lazily so awards are safe even before start() has run.
--
-- StatsService registers an award hook (ScoreService.onAward) to fold session
-- score into the persistent lifetime total and best — ScoreService itself
-- stays dependency-free so every gameplay service can require it safely.

local Players = game:GetService("Players")

local ScoreService = {}
ScoreService.name = "ScoreService"

type AwardHook = (player: Player, amount: number) -> ()
local awardHooks: { AwardHook } = {}

local function ensureScore(player: Player): IntValue
	local stats = player:FindFirstChild("leaderstats")
	if not stats then
		local folder = Instance.new("Folder")
		folder.Name = "leaderstats"
		folder.Parent = player
		stats = folder
	end
	local score = stats:FindFirstChild("Score")
	if not score then
		local value = Instance.new("IntValue")
		value.Name = "Score"
		value.Value = 0
		value.Parent = stats
		score = value
	end
	return score :: IntValue
end

-- Registers a listener fired after every award (session Score already updated).
function ScoreService.onAward(hook: AwardHook)
	table.insert(awardHooks, hook)
end

function ScoreService.award(player: Player, amount: number)
	local score = ensureScore(player)
	score.Value += amount
	for _, hook in awardHooks do
		task.spawn(hook, player, amount)
	end
end

function ScoreService.start()
	Players.PlayerAdded:Connect(function(player)
		ensureScore(player)
	end)
	for _, player in Players:GetPlayers() do
		ensureScore(player)
	end
end

return ScoreService
