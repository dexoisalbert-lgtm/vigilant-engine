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

## Tooling

- [Rokit](https://github.com/rojo-rbx/rokit) pins tools (`rokit.toml`)
- [Selene](https://kampfkarren.github.io/selene/) lints `src/`
- [StyLua](https://github.com/JohnnyMorganz/StyLua) formats `src/`

```sh
rokit install
selene src
stylua --check src
```
