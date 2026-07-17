# Doomed Airliner Co-op — Roblox Game Plan

## Context

You want a chaotic physics-based co-op Roblox game set aboard a failing commercial airliner: players share flying, cabin service, mid-air repairs, cargo triage, wing-mounted defense, and smuggling — six overlapping systems that combine into slapstick emergent play (think *Overcooked* × *PlaneCrazy* × *Barotrauma*). The repo `vigilant-engine` is empty, so this is a greenfield full-game blueprint. Iteration will happen live in Roblox Studio via the Studio MCP connection, with the repo used to snapshot the place file and script source for versioning.

The plan below defines the toolchain, architecture, core systems, content, phased build order, and verification approach so that once implementation starts, each phase has a clear scope and a testable endpoint.

## Assumptions

- **Scope**: full-game blueprint targeting all six systems as the shipped game, built in phases so the first playable slice is small.
- **Toolchain**: Roblox Studio is the primary editor, driven via the Studio MCP connection. Place-file snapshots (`.rbxlx` — XML, git-diffable) and any external script mirrors get committed to this repo.
- **Player count target**: 4–8 per server. Enough for role coverage (pilot + 3–7 crew), small enough for physics stability.
- **Aesthetic**: colorful, low-detail, ragdoll-heavy — leans into Roblox's stock look rather than fighting it.
- **Monetization/UGC**: out of scope for this plan; leave hooks in the data model but do not design shop or DevProduct flows yet.

## Toolchain & Repo Layout (repo-first)

- **Source of truth**: all game logic is written as `.luau` files in this repository, developed and reviewed in git *before* any Studio session. Studio via MCP is the deployment/iteration target, not the editor of record.
- **Deploy to Studio**: when a Studio session is available, a `tools/deploy-manifest.json` maps each `src/` file to its target Instance path (e.g. `src/server/FlightController.luau → ServerScriptService/Services/FlightController`). The MCP session walks the manifest and creates/updates Script instances with the file contents. World geometry (plane model, maps) is built in Studio via MCP, guided by builder scripts in `src/server/setup/` that programmatically construct rigs (plane assembly, cabin seats, cargo racks) so geometry is reproducible from code where practical.
- **Versioning**: `src/` is authoritative for logic. After Studio sessions that add hand-built geometry, export `place/vigilant-engine.rbxlx` and commit as a snapshot.
- **Studio-independence**: scripts must be written so they can be authored blind — each module takes its Instance dependencies via a small locator module (`src/shared/Refs.luau`) that resolves paths by `CollectionService` tags rather than hardcoded hierarchy, so code written before the world exists still binds correctly once deployed.
- **Tooling**: Selene (lint) and StyLua (format) run against `src/`, pinned via `rokit.toml`. Rojo remains an escape hatch if MCP-manifest deployment gets painful.
- **Repo layout**:
  ```
  place/                  # .rbxlx snapshots (post-Studio)
  src/
    client/               # LocalScripts, controllers
    server/               # Scripts, services
      missions/           # mission definition modules
      setup/              # programmatic world/rig builders
    shared/               # ModuleScripts used by both (Refs, FlightConfig, types)
  tools/
    deploy-manifest.json  # src file → Studio Instance path mapping
  docs/                   # design docs, mission specs
  ```

## Architecture Overview

### Client / Server Split

- **Server authoritative** for: plane physics, damage state, passenger AI, mission progression, hitreg, cargo state, contraband transactions, weather escalation. Anything a cheating client could exploit lives on the server.
- **Client authoritative** for: camera, UI, local input, cosmetic particles, sound, first-person view smoothing.
- **Shared modules**: types, constants (damage thresholds, weapon stats), utility math, tool definitions.

### Roblox Services & Building Blocks

- **Physics**: `VehicleSeat` on the cockpit for input capture; a plane `Model` with a `PrimaryPart` and `VectorForce` + `AlignOrientation` + `LinearVelocity` constraints for lift/thrust/torque. Server owns the assembly via `BasePart:SetNetworkOwner(nil)` for security, but the pilot's client gets ownership *only* for input latency smoothness — a common Roblox tradeoff; damage math still gates server-side.
- **Character replication**: standard `Humanoid` characters with `Tool` objects. Ragdoll via `BallSocketConstraint` + disabled `Motor6D` on turbulence and crashes. Consider vendoring a small `RagdollService` module.
- **Interaction**: `ProximityPrompt` on every interactable (seats, welding cracks, ice patches, fire extinguisher mounts, cargo latches, Gatling gun grips). Prompts hold `HoldDuration`, `RequiresLineOfSight`, and `MaxActivationDistance` to convey urgency.
- **Networking**: one `RemoteEvent` bus per system (`CockpitBus`, `RepairBus`, `CargoBus`, etc.) instead of dozens of individual remotes. Payloads are minimal (`{action, targetGuid, ...args}`). Server validates every payload.
- **Streaming**: `Workspace.StreamingEnabled = true` with a generous radius so long maps (arctic, forest runway, ocean) don't blow client memory.
- **Rounds**: a `MissionService` on the server runs mission scripts (ModuleScripts under `src/server/missions/`). Each mission declares spawn, weather, hostile schedule, cargo manifest, win/lose conditions.

## Core Systems

Each system has: an interaction surface (what the player touches), server logic (what runs authoritatively), and failure states (what makes it interesting).

### 1. Cockpit & Flight Management

- **Interaction**: VehicleSeat drives throttle + steer via built-in properties; supplementary `ProximityPrompt`s on landing-gear lever, flap switch, autopilot toggle, engine start, fuel-transfer switch. First-person camera locks to the pilot seat with head-look freelook.
- **Server**: `FlightController` module reads `VehicleSeat.Throttle` / `.Steer`, plus attribute state on the cockpit part for gear/flaps/autopilot, and emits target thrust / pitch / roll / yaw to the plane assembly via constraints each `Heartbeat`. Physics tuning (mass, inertia, wing lift curves) lives in one `FlightConfig` module so it's tunable in one place.
- **Instruments**: a `SurfaceGui` cluster on the cockpit dashboard shows altitude, airspeed, heading, vertical speed, fuel, engine temp, damage summary. Values pulled from plane attributes via `GetAttributeChangedSignal`.
- **Takeoff/landing**: runway parts get tagged with `CollectionService` tag `"Runway"`; touchdown physics considers a soft-landing threshold — under it, no damage; over it, gear detaches.
- **Aerial hazards**:
  - **Birds** — flocks are a `Model` of small parts with a `FlockController` module doing simple boids on the server. Collision with cockpit window triggers a shatter effect (swap window part for pre-fractured version), engine ingestion drops that engine's thrust.
  - **Storms/lightning** — see system 3.

### 2. Cabin Crew & Passenger Relations

- **Passengers**: NPC `Humanoid`s spawned into cabin seats at mission start. A `PassengerAI` module cycles each NPC through states: `Seated`, `Wanting` (food/drink/attention), `Panicking` (turbulence, hull breach nearby), `Ragdolled` (thrown around), `Enraged` (unmet demands too long → attacks crew).
- **Buckling**: seatbelt is a `ProximityPrompt` on each seat with a short hold; when buckled, a `WeldConstraint` locks the passenger to the seat and they survive turbulence without ragdolling.
- **Service items**: `Tool`s for tray, soda can, coffee cup, burger. Delivering the right item to a Wanting passenger resolves the demand and grants score. Tools have per-instance state (`Attributes.Contents = "coffee"`), and physics — trays tip if held while sprinting through turbulence.
- **Coffee-break-plus**: the coffee cup Tool has a `SelfServe` action that lets a crew member sip it themselves, and a `Yeet` action that lets them jump out of the plane while holding it — literal comedy escape.
- **Turbulence**: on turbulence tick, unbuckled seated passengers get a random impulse applied via `LinearVelocity` pulse; standing crew get a wobble applied via `Humanoid.Sit = true` briefly or a targeted ragdoll.

### 3. Emergency Maintenance & Crisis Control

- **Damage model**: every repairable Instance carries three attributes — `Damage: number`, `State: string ("Ok" | "Cracked" | "OnFire" | "Iced" | "Detached")`, `RepairKind: string ("Weld" | "Hammer" | "Extinguisher" | "IceAxe")`. A single `DamageService` mutates these; UI and systems only read.
- **Tools**: `Blowtorch` (heals `Cracked` hull parts, ProximityPrompt on cracks), `Hammer` (heals structural parts / knocks bent panels), `FireExtinguisher` (clears `OnFire`, held Tool with a hose spray raycast), `IceAxe` (climbs onto wings, hacks ice — see system 5).
- **Failure escalation**: undressed damage compounds. Cracked hull → depressurization pulls unbuckled entities toward the breach via a per-tick `LinearVelocity` on nearby characters. Uncleared engine fire → engine detaches. Uncleared ice → weight increases, flight ceiling drops.
- **Lightning strikes**: mission scheduler in storm zones triggers a `LightningEvent` on a random plane part; VFX bolt + damage bump + chance of engine fire.

### 4. Cargo & Baggage

- **Hold layout**: a rear cargo section with `PrismaticConstraint` ramp door. Cargo parts spawn on racks with `Attachment` + `RopeConstraint` "straps" — cutting/unlatching frees them to slide.
- **Cargo classes**: `Luggage`, `Crate`, `Piano`, `Livestock`, `Wildlife`, `Contraband`. Each is a `Tool`-able physical part with mass/value tags.
- **Livestock/wildlife**: `Chicken` and `Crocodile` NPCs use lightweight AI (chickens flap, crocs charge). Cage integrity is a damage-tracked part; broken cage frees the animal into the cabin — chaos vector.
- **Overweight**: plane assembly `TotalMass` computed from cargo attribute sum. Above a mission-set threshold, `FlightController` caps ceiling and rate-of-climb — motivating the jettison mechanic.
- **Jettison**: rear ramp open + push cargo aft with a `LinearVelocity` on the cargo hold floor, or manually `Tool`-grab and throw. Ejected cargo despawns after leaving streaming radius.

### 5. Wing-Walking Combat

- **Egress**: cabin doors have `ProximityPrompt`s that toggle "outside mode" — the character gets a temporary `AlignOrientation` matching the plane's frame and increased `Humanoid.HipHeight` clamp so they stick to the wing surface. Losing footing ragdolls them into the sky (respawn at nearest occupied seat, or die if the mission is hardcore).
- **Gatling gun**: mounted `Model` with a `Seat` on top. Sitting activates a first-person aiming camera and gives the player a `WeaponController` client-side, but hitreg happens server-side: client sends `{fire, targetCFrame}`, server raycasts against enemy planes with a small forgiveness cone and applies damage.
- **Ice axe on hull**: while outside, the `IceAxe` Tool targets `Iced` parts on the fuselage or wings. Each hack lowers ice mass; enough hacks clears the part.
- **Hostile fighters**: server AI planes spawned by mission scheduler. Simple pursuit + strafe pattern, fire tracer bolts that damage plane parts.

### 6. Smuggling & Law Enforcement

- **Contraband**: taped packages spawn in a hidden compartment. Packages have a `DropZoneId` — they must be jettisoned over a matching ground zone (a tagged part in the world) to score.
- **Onboard security NPC**: some missions spawn an `AirMarshal` NPC that patrols the cabin. Their AI notices contraband held in the open, weapons drawn, or players entering the smuggling compartment. Detection triggers arrest attempt: they cuff a random offender who's then locked in a seat for N seconds.
- **Ground accomplices**: cosmetic — a truck at the drop zone flashes lights when a package lands within radius.
- **Mission-gated**: smuggling only appears in Smuggler-type missions; law enforcement is not a persistent world nuisance.

## Data Model

- **Plane state** (Instance attributes on the plane Model):
  `EngineHealth[1..4]`, `HullIntegrity`, `IceMass`, `FuelKg`, `Altitude`, `Airspeed`, `AutopilotOn`, `GearDown`, `FlapsDeg`.
- **Damage state** (per repairable part): `Damage`, `State`, `RepairKind` — as above.
- **Player role** (on `Player`): `Role: "Pilot" | "Crew"`, `Score`, `RoundStats` (folder of numeric attributes).
- **Mission state** (server singleton in `ReplicatedStorage.MissionState`): `MissionId`, `Phase ("Boarding" | "Takeoff" | "Cruise" | "Crisis" | "Landing" | "Ended")`, `SecondsRemaining`, `WinCondition`.

Persistent data (across sessions) is deferred to a later phase. When it lands, use `DataStoreService` behind a `ProfileService`-style wrapper — do not touch DataStore directly.

## Content Plan

- **Aircraft** (start with one, expand to three):
  1. `Boeing 707-ish twin-engine narrowbody` — the default doomed airliner.
  2. `Cargo turboprop` — smaller, twitchier, for smuggling missions.
  3. `Wide-body quad` — the "everything goes wrong at once" endgame plane.
- **Maps / routes** (one per phase minimum):
  1. `Forest dirt runway` — takeoff/landing skill test.
  2. `Arctic corridor` — ice buildup, wing-walking axe work.
  3. `Storm belt` — lightning, birds, turbulence.
  4. `Ocean smuggler run` — drop zones, hostile interceptors.
  5. `Mountain pass` — narrow flying, terrain collision risk.
- **Mission types**: `Delivery` (get to destination alive), `Smuggle` (drop N packages in zones), `Defend` (survive N hostile waves), `Rescue` (pick up passengers mid-route), `Chaos` (everything, gauntlet).
- **NPC roster**: passengers (business, tourist, karen, child, celebrity), animals (chicken, crocodile, pig), hostile pilots, air marshal, ground accomplice.

## Phased Build Order

Each phase ends in a playable, testable state. Do not start phase N+1 until phase N's verification passes.

Phases 0–1 are written repo-first: all scripts authored as `.luau` in `src/` with the deploy manifest, committed and pushed, *before* any Studio session. Later phases follow the same pattern — code lands in git first, then gets deployed to Studio via MCP for playtesting.

- **Phase 0 — Repo skeleton & core modules (no Studio needed).** Create the repo layout, `rokit.toml`, `selene.toml`, `stylua.toml`, `tools/deploy-manifest.json`, and the foundational shared modules: `Refs.luau` (tag-based instance locator), `FlightConfig.luau`, `Types.luau`, the RemoteEvent bus helper, and stubs for each service. Verify: Selene and StyLua pass; manifest paths are consistent with `src/`.
- **Phase 1 — Flyable plane (core of system 1).** Write `FlightController`, `PlaneRig` builder (programmatically constructs the plane model, VehicleSeat, constraints), and the instruments client script in `src/`; commit. Then deploy via MCP into a Studio session with one flat runway map. Verify: two players can join, one flies, other spectates, plane takes off and lands.
- **Phase 2 — Cabin & passengers (system 2 minimal).** Cabin seats, buckling prompt, one `PassengerAI` state machine, tray/soda/coffee tools, service scoring. Verify: 3 players, one flies, two serve, passengers get demands met, coffee-yeet works.
- **Phase 3 — Damage & repair (system 3).** `DamageService`, blowtorch/hammer/extinguisher tools, cracks/fire/ice attributes, escalation. Add depressurization pull. Verify: crew can repair a triggered hull breach mid-flight without crashing.
- **Phase 4 — Cargo (system 4).** Hold layout, cargo classes, ramp, jettison, overweight ceiling cap, chicken/croc NPCs. Verify: an overweight plane jettisons enough to climb again; a freed crocodile ragdolls a passenger.
- **Phase 5 — Wing combat (system 5).** Outside-mode locomotion on wings, Gatling gun seat, hostile fighter AI, ice-axe on hull. Verify: two players outside, one on Gatling shoots down a fighter, other clears wing ice.
- **Phase 6 — Missions & smuggling (system 6 + `MissionService`).** Mission scripts for the five map types, smuggler compartment, air marshal AI, drop zones. Verify: a full Smuggler mission plays start-to-finish with win/lose states.
- **Phase 7 — Polish & second aircraft.** Ragdoll pass, sound pass, VFX pass (lightning, engine fire, muzzle flash), second plane variant, second map polish.
- **Phase 8 — Persistence & live-ops hooks.** `ProfileService` wrapper, per-player stats, leaderboards. Defer shop/DevProducts unless requested.

## Verification

- **Per-phase**: end-to-end play in Studio via MCP with 2–8 test players (Studio's local server + Play Solo emulated clients or `Team Test`). Each phase's Verify bullet above is the acceptance check.
- **Physics stability regression**: after any change to `FlightController` or plane assembly, run a scripted 5-minute autopilot cruise and confirm no NaN CFrames, no runaway spin, no clientside jitter above threshold.
- **Multiplayer regression**: after any change to a system's RemoteEvent bus, run a 4-player Team Test that exercises that system's happy path plus one adversarial input (malformed payload from a fake client) and confirm the server rejects cleanly.
- **Performance budget**: keep client script frame time under 4 ms in a fully-populated round on medium hardware. Use Studio's MicroProfiler; if exceeded, streaming radius or NPC AI tick rate are the first knobs.
- **Content review**: each new mission gets one full playtest with the max player count before merging.
