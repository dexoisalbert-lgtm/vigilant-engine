--!strict
-- Minimal session-locked profile store (Phase 8), a compact ProfileService-
-- style wrapper over DataStoreService. Server-only in practice (DataStore APIs
-- don't run on clients), kept in shared alongside the other libraries.
--
-- Session lock: each stored record carries a `lock = { session, time }`. A
-- LoadAsync claims the key via UpdateAsync only if no other LIVE session holds
-- it (a lock older than LockTimeoutSeconds is considered dead and stolen).
-- Save/Release refuse to clobber a record another live session owns, so two
-- servers can never trample each other's data.
--
-- Graceful degradation: if the DataStore API is unavailable — a Studio session
-- without "Enable Studio Access to API Services", or an offline test — the
-- store transparently switches to an in-memory table. The game runs normally;
-- it just doesn't persist across sessions. This keeps first-boot deploys from
-- erroring before anyone has flipped the API-access switch.

local DataStoreService = game:GetService("DataStoreService")
local HttpService = game:GetService("HttpService")

local PersistenceConfig = require(script.Parent:WaitForChild("PersistenceConfig"))

local SESSION_TAG = HttpService:GenerateGUID(false)

local ProfileStore = {}
ProfileStore.__index = ProfileStore

local Profile = {}
Profile.__index = Profile

type StoredData = { [string]: any }

export type Profile = {
	key: string,
	data: StoredData,
	Save: (self: Profile) -> boolean,
	Release: (self: Profile) -> (),
}

local function deepCopy(source: { [string]: any }): { [string]: any }
	local result = {}
	for key, value in source do
		if typeof(value) == "table" then
			result[key] = deepCopy(value)
		else
			result[key] = value
		end
	end
	return result
end

local function lockIsLive(lock: any): boolean
	return typeof(lock) == "table"
		and lock.session ~= SESSION_TAG
		and typeof(lock.time) == "number"
		and (os.time() - lock.time) < PersistenceConfig.LockTimeoutSeconds
end

local function backfillDefaults(data: StoredData)
	for key, value in PersistenceConfig.Defaults :: { [string]: any } do
		if data[key] == nil then
			data[key] = value
		end
	end
end

function ProfileStore.new(storeName: string)
	local self = setmetatable({}, ProfileStore)
	self.mock = false
	self.memory = {} :: { [string]: StoredData }
	local ok, store = pcall(function()
		return DataStoreService:GetDataStore(storeName)
	end)
	if ok then
		self.store = store
	else
		self.mock = true
		warn("[VE] ProfileStore: DataStore unavailable, using in-memory store")
	end
	return self
end

function ProfileStore:_makeProfile(key: string, data: StoredData): Profile
	backfillDefaults(data)
	return setmetatable({
		key = key,
		data = data,
		_store = self,
		_released = false,
	}, Profile) :: any
end

function ProfileStore:LoadAsync(key: string): Profile?
	if self.mock then
		local data = self.memory[key]
		if not data then
			data = deepCopy(PersistenceConfig.Defaults :: any)
			self.memory[key] = data
		end
		return self:_makeProfile(key, data)
	end

	for attempt = 1, PersistenceConfig.LoadRetries do
		local claimed: StoredData? = nil
		local ok, err = pcall(function()
			self.store:UpdateAsync(key, function(old)
				old = old or {}
				old.data = old.data or {}
				if lockIsLive(old.lock) then
					return nil -- another live session owns it; cancel this write
				end
				old.lock = { session = SESSION_TAG, time = os.time() }
				claimed = old.data
				return old
			end)
		end)
		if not ok then
			warn("[VE] ProfileStore load failed, falling back to memory: " .. tostring(err))
			self.mock = true
			return self:LoadAsync(key)
		end
		if claimed then
			return self:_makeProfile(key, claimed)
		end
		task.wait(PersistenceConfig.RetryBackoffSeconds * attempt)
	end
	return nil -- locked by a live session that never released
end

function Profile.Save(self: any): boolean
	local store = self._store
	if store.mock then
		store.memory[self.key] = self.data
		return true
	end
	local ok, err = pcall(function()
		store.store:UpdateAsync(self.key, function(old)
			old = old or {}
			if lockIsLive(old.lock) then
				return nil -- don't overwrite another live session's record
			end
			old.data = self.data
			old.lock = { session = SESSION_TAG, time = os.time() }
			return old
		end)
	end)
	if not ok then
		warn("[VE] ProfileStore save failed for " .. self.key .. ": " .. tostring(err))
	end
	return ok
end

function Profile.Release(self: any)
	if self._released then
		return
	end
	self._released = true
	local store = self._store
	if store.mock then
		store.memory[self.key] = self.data
		return
	end
	pcall(function()
		store.store:UpdateAsync(self.key, function(old)
			old = old or {}
			if lockIsLive(old.lock) then
				return nil
			end
			old.data = self.data
			old.lock = nil -- clear the lock so the next session can claim it
			return old
		end)
	end)
end

return ProfileStore
