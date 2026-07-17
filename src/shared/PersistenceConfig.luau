--!strict
-- Single tuning surface for persistence (docs/GAME_PLAN.md, Phase 8).
-- Bump StoreName's version suffix to wipe live data after a breaking schema
-- change. Shop / DevProduct economy is intentionally out of scope for now.

local PersistenceConfig = {
	StoreName = "VE_PlayerProfiles_v1",
	KeyPrefix = "Player_", -- key = KeyPrefix .. UserId

	AutosaveIntervalSeconds = 120,
	LockTimeoutSeconds = 900, -- a session lock older than this is treated as dead
	LoadRetries = 5,
	RetryBackoffSeconds = 3, -- multiplied by attempt number

	-- Persistent stat schema and starting values. Every field here is written
	-- into a fresh profile and back-filled into older ones on load.
	Defaults = {
		TotalScore = 0, -- lifetime cumulative score
		Best = 0, -- highest single-session score
		Wins = 0, -- missions completed
		FightersDowned = 0,
	},

	-- Persistent values mirrored into leaderstats for the in-experience
	-- leaderboard, in display order. Session "Score" is added separately by
	-- ScoreService and is not persistent.
	LeaderstatMirror = { "Wins", "Best" },
}

return PersistenceConfig
