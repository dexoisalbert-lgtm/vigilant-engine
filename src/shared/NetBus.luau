--!strict
-- One RemoteEvent per named channel ("Cockpit", "Repair", "Cargo", ...) with
-- action-string dispatch, instead of dozens of individual remotes.
--
-- Server:  local bus = NetBus.server("Cockpit")
--          bus.on("Input", function(player, data) ... end)
--          bus.fireAll("PhaseChanged", { phase = "Takeoff" })
-- Client:  local bus = NetBus.client("Cockpit")
--          bus.fire("Input", { p = 1, r = 0 })
--          bus.on("PhaseChanged", function(data) ... end)
--
-- Payloads are always (action: string, data: table?). The server side drops
-- malformed payloads; every handler must still validate its own fields.

local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local FOLDER_NAME = "VE_Remotes"

type Data = { [string]: any }
type ServerHandler = (player: Player, data: Data?) -> ()
type ClientHandler = (data: Data?) -> ()

export type ServerChannel = {
	on: (action: string, handler: ServerHandler) -> (),
	fire: (player: Player, action: string, data: Data?) -> (),
	fireAll: (action: string, data: Data?) -> (),
}

export type ClientChannel = {
	on: (action: string, handler: ClientHandler) -> (),
	fire: (action: string, data: Data?) -> (),
}

local NetBus = {}

local serverChannels: { [string]: ServerChannel } = {}
local clientChannels: { [string]: ClientChannel } = {}

local function getFolder(): Folder
	if RunService:IsServer() then
		local folder = ReplicatedStorage:FindFirstChild(FOLDER_NAME)
		if not folder then
			folder = Instance.new("Folder")
			folder.Name = FOLDER_NAME
			folder.Parent = ReplicatedStorage
		end
		return folder :: Folder
	end
	return ReplicatedStorage:WaitForChild(FOLDER_NAME) :: Folder
end

local function getRemote(channelName: string): RemoteEvent
	local folder = getFolder()
	if RunService:IsServer() then
		local remote = folder:FindFirstChild(channelName)
		if not remote then
			remote = Instance.new("RemoteEvent")
			remote.Name = channelName
			remote.Parent = folder
		end
		return remote :: RemoteEvent
	end
	return folder:WaitForChild(channelName) :: RemoteEvent
end

function NetBus.server(channelName: string): ServerChannel
	assert(RunService:IsServer(), "NetBus.server may only be called on the server")
	local existing = serverChannels[channelName]
	if existing then
		return existing
	end

	local remote = getRemote(channelName)
	local handlers: { [string]: { ServerHandler } } = {}

	remote.OnServerEvent:Connect(function(player: Player, action: unknown, data: unknown)
		if typeof(action) ~= "string" then
			return
		end
		if data ~= nil and typeof(data) ~= "table" then
			return
		end
		local list = handlers[action :: string]
		if not list then
			return
		end
		for _, handler in list do
			task.spawn(handler, player, data :: Data?)
		end
	end)

	local channel: ServerChannel = {
		on = function(action, handler)
			local list = handlers[action]
			if not list then
				list = {}
				handlers[action] = list
			end
			table.insert(list, handler)
		end,
		fire = function(player, action, data)
			remote:FireClient(player, action, data)
		end,
		fireAll = function(action, data)
			remote:FireAllClients(action, data)
		end,
	}
	serverChannels[channelName] = channel
	return channel
end

function NetBus.client(channelName: string): ClientChannel
	assert(not RunService:IsServer(), "NetBus.client may only be called on the client")
	local existing = clientChannels[channelName]
	if existing then
		return existing
	end

	local remote = getRemote(channelName)
	local handlers: { [string]: { ClientHandler } } = {}

	remote.OnClientEvent:Connect(function(action: unknown, data: unknown)
		if typeof(action) ~= "string" then
			return
		end
		if data ~= nil and typeof(data) ~= "table" then
			return
		end
		local list = handlers[action :: string]
		if not list then
			return
		end
		for _, handler in list do
			task.spawn(handler, data :: Data?)
		end
	end)

	local channel: ClientChannel = {
		on = function(action, handler)
			local list = handlers[action]
			if not list then
				list = {}
				handlers[action] = list
			end
			table.insert(list, handler)
		end,
		fire = function(action, data)
			remote:FireServer(action, data)
		end,
	}
	clientChannels[channelName] = channel
	return channel
end

return NetBus
