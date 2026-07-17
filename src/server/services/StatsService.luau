--!strict
-- StatsService — Phase 8 (docs/GAME_PLAN.md, persistence & live-ops hooks).
-- Owns the per-player persistent profile lifecycle and the leaderboard.
--
-- On join: loads a session-locked profile (kicks the player if another live
-- server holds the lock, standard ProfileService behavior) and mirrors the
-- persistent Wins/Best stats into leaderstats beside the session Score.
-- On leave / shutdown: saves and releases the lock. Autosaves periodically.
--
-- Session score folds into the lifetime total and best automatically via the
-- ScoreService award hook, so no gameplay service needs to know persistence
-- exists. Category stats (Wins, FightersDowned) are bumped by
-- StatsService.addStat, which no-ops safely if the profile isn't loaded yet.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local Shared = game:GetService("ReplicatedStorage"):WaitForChild("VEShared")
local ProfileStore = require(Shared:WaitForChild("ProfileStore"))
local PersistenceConfig = require(Shared:WaitForChild("PersistenceConfig"))
local ScoreService = require(script.Parent:WaitForChild("ScoreService")) :: any

local StatsService = {}
StatsService.name = "StatsService"

type Entry = {
	profile: ProfileStore.Profile,
	mirrors: { [string]: IntValue },
}

local store = ProfileStore.new(PersistenceConfig.StoreName)
local entries: { [Player]: Entry } = {}

local function ensureLeaderstats(player: Player): Folder
	local stats = player:FindFirstChild("leaderstats")
	if not stats then
		local folder = Instance.new("Folder")
		folder.Name = "leaderstats"
		folder.Parent = player
		stats = folder
	end
	return stats :: Folder
end

local function ensureValue(parent: Folder, name: string, value: number): IntValue
	local existing = parent:FindFirstChild(name)
	if existing and existing:IsA("IntValue") then
		existing.Value = value
		return existing
	end
	local created = Instance.new("IntValue")
	created.Name = name
	created.Value = value
	created.Parent = parent
	return created
end

-- Bumps a persistent stat (creating it if the schema grew) and updates its
-- leaderstats mirror when there is one. Safe before the profile has loaded.
function StatsService.addStat(player: Player, key: string, amount: number)
	local entry = entries[player]
	if not entry then
		return
	end
	local current = entry.profile.data[key]
	entry.profile.data[key] = (if typeof(current) == "number" then current else 0) + amount
	local mirror = entry.mirrors[key]
	if mirror then
		mirror.Value = entry.profile.data[key]
	end
end

function StatsService.get(player: Player, key: string): number
	local entry = entries[player]
	if not entry then
		return 0
	end
	local value = entry.profile.data[key]
	return if typeof(value) == "number" then value else 0
end

local function onPlayerAdded(player: Player)
	local profile = store:LoadAsync(PersistenceConfig.KeyPrefix .. tostring(player.UserId))
	if not profile then
		player:Kick("Your save is still active in another session. Please rejoin in a minute.")
		return
	end
	if not player.Parent then
		-- Left during the load yield; release the lock we just took.
		profile:Release()
		return
	end

	local stats = ensureLeaderstats(player)
	ensureValue(stats, "Score", 0) -- session score, owned by ScoreService
	local mirrors: { [string]: IntValue } = {}
	for _, key in PersistenceConfig.LeaderstatMirror do
		local value = profile.data[key]
		mirrors[key] = ensureValue(stats, key, if typeof(value) == "number" then value else 0)
	end

	entries[player] = { profile = profile, mirrors = mirrors }
end

local function onPlayerRemoving(player: Player)
	local entry = entries[player]
	if not entry then
		return
	end
	entries[player] = nil
	entry.profile:Release() -- Release writes the final data too
end

function StatsService.start()
	-- Fold every session award into the lifetime total and running best.
	ScoreService.onAward(function(player: Player, amount: number)
		local entry = entries[player]
		if not entry then
			return
		end
		entry.profile.data.TotalScore = (entry.profile.data.TotalScore or 0) + amount
		local stats = player:FindFirstChild("leaderstats")
		local score = stats and stats:FindFirstChild("Score")
		local sessionScore = if score and score:IsA("IntValue") then score.Value else 0
		if sessionScore > (entry.profile.data.Best or 0) then
			entry.profile.data.Best = sessionScore
			local bestMirror = entry.mirrors.Best
			if bestMirror then
				bestMirror.Value = sessionScore
			end
		end
	end)

	Players.PlayerAdded:Connect(function(player)
		task.spawn(onPlayerAdded, player)
	end)
	Players.PlayerRemoving:Connect(onPlayerRemoving)
	for _, player in Players:GetPlayers() do
		task.spawn(onPlayerAdded, player)
	end

	task.spawn(function()
		while true do
			task.wait(PersistenceConfig.AutosaveIntervalSeconds)
			for _, entry in entries do
				entry.profile:Save()
			end
		end
	end)

	game:BindToClose(function()
		for player, entry in entries do
			entries[player] = nil
			entry.profile:Release()
		end
		-- Give the release UpdateAsyncs a beat to flush before the box dies.
		if not RunService:IsStudio() then
			task.wait(2)
		end
	end)
end

return StatsService
