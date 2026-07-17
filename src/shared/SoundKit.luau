--!strict
-- Sound pass scaffolding (Phase 7): one place for every sound id plus tiny
-- play helpers. Only guaranteed-available rbxasset built-ins ship as
-- defaults; empty ids are silent no-ops, to be filled with uploaded
-- rbxassetid:// ids during the Studio polish session.

local Debris = game:GetService("Debris")

local SoundKit = {}

SoundKit.Ids = {
	Ping = "rbxasset://sounds/electronicpingshort.wav", -- toast/notify
	Ouch = "rbxasset://sounds/uuhhh.mp3", -- comedic impact
	Explosion = "", -- TODO(studio): fighter kill boom
	EngineLoop = "", -- TODO(studio): looping engine rumble, pitch by throttle
	Weld = "", -- TODO(studio): blowtorch sizzle
	Alarm = "", -- TODO(studio): hazard klaxon
}

-- Plays a positional one-shot by dropping a part with PlayOnRemove.
function SoundKit.playAt(position: Vector3, id: string, volume: number?)
	if id == "" then
		return
	end
	local holder = Instance.new("Part")
	holder.Anchored = true
	holder.CanCollide = false
	holder.CanQuery = false
	holder.Transparency = 1
	holder.Size = Vector3.one
	holder.CFrame = CFrame.new(position)
	local sound = Instance.new("Sound")
	sound.SoundId = id
	sound.Volume = volume or 0.7
	sound.PlayOnRemove = true
	sound.Parent = holder
	holder.Parent = workspace
	holder:Destroy()
end

-- Plays a one-shot parented to an instance (a part for 3D, a gui for global).
function SoundKit.playIn(parent: Instance, id: string, volume: number?)
	if id == "" then
		return
	end
	local sound = Instance.new("Sound")
	sound.SoundId = id
	sound.Volume = volume or 0.5
	sound.Parent = parent
	sound:Play()
	Debris:AddItem(sound, 8)
end

return SoundKit
