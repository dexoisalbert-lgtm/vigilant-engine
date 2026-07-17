# Vigilant Engine — Doomed Airliner Co-op

A chaotic, physics-based co-op Roblox game aboard a failing commercial airliner.
Players share flying, cabin service, mid-air repairs, cargo triage, wing-mounted
defense, and smuggling. Full design: [`docs/GAME_PLAN.md`](docs/GAME_PLAN.md).

## Workflow (repo-first)

All game logic is authored as `.luau` files in `src/` and reviewed in git.
Roblox Studio (via the Studio MCP connection) is the deployment target, not the
editor of record: `tools/deploy-manifest.json` maps every source file to its
target Instance path, and an MCP Studio session walks the manifest to
create/update Script instances. World geometry is constructed programmatically
by the builders in `src/server/setup/` on first boot, so an empty baseplate
place is enough to get a flyable game.

Deployment into Studio happens only on explicit request — nothing in this repo
touches Studio by itself.

## Layout

```
place/                  # .rbxlx snapshots exported after Studio sessions
src/
  client/               # LocalScript entry point (StarterPlayerScripts)
    controllers/        # client controller modules
  server/               # Script entry point (ServerScriptService)
    services/           # server service modules (FlightController, stubs)
    setup/              # programmatic world/rig builders (PlaneRig, Runway)
    missions/           # mission definition modules (Phase 6)
  shared/               # ModuleScripts used by both (ReplicatedStorage)
tools/
  deploy-manifest.json  # src file -> Studio Instance path mapping
docs/                   # design docs, mission specs
```

## Pilot controls (Phase 1)

| Input | Action |
| --- | --- |
| Sit on the cockpit `VehicleSeat` | Become the pilot |
| `S` / `W` | Pitch up / pitch down |
| `A` / `D` | Roll left / right (nosewheel steering on the ground) |
| `Left Shift` / `Left Ctrl` | Throttle up / down |
| Dashboard proximity prompt | Toggle autopilot (holds current altitude, wings level) |

The dashboard instrument cluster (speed, altitude, vertical speed, heading,
throttle, fuel) renders on the `VE_CockpitDash` part and updates live from
plane attributes.

## Cabin crew (Phase 2)

| Interaction | Effect |
| --- | --- |
| Galley dispensers (rear counter) | Take a Coffee / Soda / Burger / Tray tool |
| "Serve" prompt on a passenger | Deliver the item they're demanding (+score) |
| "Buckle Seatbelt" prompt on a seat | Protects the occupant (player or NPC) from turbulence |
| Click with Coffee equipped | Sip: brief caffeination speed buff (3 sips per cup) |
| Sip Coffee in freefall off a moving plane | Coffee Break Plus: big score, broadcast to everyone |

Passengers demand items on a timer and enrage if ignored — enraged passengers
wander the cabin and shove crew. Periodic turbulence tosses every unbuckled
occupant (players included); buckled ones are safe. Passengers who get thrown
out of the plane do not survive the experience.

## Maintenance crew (Phase 3)

While airborne, hazards fire at random: hull cracks, engine fires, wing
icing, and lightning strikes. Repair tools come from the red locker beside
the cockpit; each damaged part shows a repair prompt that only works with the
matching tool equipped (+score on completion).

| Damage | Tool | If ignored |
| --- | --- | --- |
| Cracked hull panel | Blowtorch (weld) | Panel tears off entirely |
| Missing panel / dead engine mount | Hammer | Permanent breach / lost thrust |
| Engine fire | Fire Extinguisher | Engine health drains to 0, engine detaches |
| Iced wing | Ice Axe (multiple hacks) | Ice weight drags the plane down |

Cracked or missing hull panels depressurize the cabin: anyone unbuckled near
the breach gets pulled toward it — hold onto something. Engine health feeds
straight into available thrust, and detached engines can be rebuilt (to 30%)
by hammering the empty mount. `HullIntegrity` and `IceMass` are published as
attributes on the plane model.

## Cargo crew (Phase 4)

The aft cargo bay (through the bulkhead door behind the galley) spawns
strapped-down cargo: crates, a piano, luggage, a chicken crate, and a
crocodile cage. The default loadout is over the plane's cargo capacity — an
overweight plane can barely climb until someone jettisons the heavy stuff.

| Interaction | Effect |
| --- | --- |
| "Unstrap" / "Strap Down" prompt on cargo | Frees it to slide (or secures it again inside the hold) |
| Ramp prompt at the rear | Opens/closes the ramp; loose cargo gets sucked out aft in flight |
| Touch luggage | Pick it up as a tool; click to hurl it (out the ramp, ideally) |
| "Catch Chicken" / "Wrangle Croc" prompts | Recapture escaped animals (+score) |

Cages burst on hard impacts once unstrapped. Chickens hop around underfoot;
the crocodile hunts the nearest player or passenger and knocks them flat.
`CargoMass` and `ExcessCargoMass` are published on the plane model, and
excess weight subtracts directly from lift.

## Wing combat (Phase 5)

Walk out the cabin door onto the wings — a client-side harness carries you
with the plane's motion while you stand on its exterior (tunable via
`CombatConfig.WingStickStrength`). Each wing has a mounted Gatling turret.

| Interaction | Effect |
| --- | --- |
| Sit on a wing turret seat | First-person gunner view |
| Hold Mouse 1 / F (touch button on mobile) | Fire; server-side hitreg with a small forgiveness cone |
| Ice Axe on iced wings | Same Phase 3 repair flow, now under fire |

Hostile fighters spawn in waves while the plane is at speed: they orbit and
strafe, firing tracer bolts that wound crew and can crack hull panels or set
engines on fire. Shooting one down awards the kill score; slowing the plane
below disengage speed makes them break off.

## Tooling

- [Rokit](https://github.com/rojo-rbx/rokit) pins tools (`rokit.toml`)
- [Selene](https://kampfkarren.github.io/selene/) lints `src/`
- [StyLua](https://github.com/JohnnyMorganz/StyLua) formats `src/`

```sh
rokit install
selene src
stylua --check src
```
