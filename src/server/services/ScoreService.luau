--!strict
-- Per-player scoring via the standard leaderstats folder. Other services call
-- ScoreService.award(); the leaderstats IntValue is created lazily so awards
-- are safe even before start() has run.

local Players = game:GetService("Players")

local ScoreService = {}
ScoreService.name = "ScoreService"

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

function ScoreService.award(player: Player, amount: number)
	local score = ensureScore(player)
	score.Value += amount
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
