# Paleotrench

First-person underwater horror in Unreal Engine 5. A scuba diver is pulled
through a rift into a prehistoric cave system and must kill four ancient
predators and repair a time machine before the oxygen runs out.

- **Design and roadmap:** [Docs/GDD.md](Docs/GDD.md)
- **Setting up your PC:** [Docs/SETUP.md](Docs/SETUP.md)
- **Current milestone checklist:** [Docs/Playtests/M0.md](Docs/Playtests/M0.md)

## Status

| Milestone | State |
|---|---|
| M0 — project skeleton, build scripts, core test harness | written; waiting for first compile on a real machine |
| M1 — swimming, camera, underwater look | next |

## Layout

```
Paleotrench.uproject   open this in Unreal
Source/PaleotrenchCore engine-free logic (random streams, seeds; later caves, resources, saves)
Source/Paleotrench     the game
Source/PaleotrenchEditor editor-only tooling
Config/                project settings
Content/               Unreal assets (Git LFS)
Data/                  tuning tables (CSV/JSON)
Tools/                 Build.bat, CollectLogs.bat, core tests
Docs/                  design, setup, playtest checklists
```

The previous project in this repository (a Roblox plane game) is preserved in
the git history; its last commit is `32886ff`.
