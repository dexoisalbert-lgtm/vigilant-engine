# Paleotrench

First-person underwater horror in Unreal Engine 5. A scuba diver is pulled
through a rift into a prehistoric cave system and must kill four ancient
predators and repair a time machine before the oxygen runs out.

- **Design and roadmap:** [Docs/GDD.md](Docs/GDD.md)
- **Setting up your PC:** [Docs/SETUP.md](Docs/SETUP.md)
- **Current milestone checklist:** [Docs/Playtests/M1.md](Docs/Playtests/M1.md)

## Status

| Milestone | State |
|---|---|
| M0 — project skeleton, build scripts, core test harness | core tests pass; first Unreal compile remains unconfirmed |
| M1 — swimming, camera, KB/M + gamepad, underwater test tank | source implemented; 25 core tests pass; Unreal compile, asset generation and playtest pending |
| M2 — oxygen, batteries, wrist computer, save/load | next after M1 acceptance |

## Try M1

Use branch **`ccr-c50880a2-g0siyj`**, with Unreal **5.7**. Close the editor,
run `Tools/Build.bat`, then `Tools/PrepareM1.bat`. Open `Paleotrench.uproject`
and press **Play**. The preparation step creates the tank map and tuning
DataTable; no manually wired Blueprints or downloaded models are required.

The diver has camera-relative movement in all three axes, quaternion
pitch/yaw/roll, inertia, drag, sprinting and swept collision. The test tank
contains pillars, a slope, elevated gates and depth markers. A fixed-power
headlamp, blue-green fog and colour grading establish the first underwater
look. Battery costs, walking in air pockets, audio and final water VFX come
in later milestones.

Controls, acceptance checks and tuning instructions are in
[M1.md](Docs/Playtests/M1.md). `Data/SwimTuning.csv` controls swimming and
camera feel; rerun `PrepareM1.bat` after editing it.

Engine-free verification: `bash Tools/run_core_tests.sh` (CMake 3.16+ and a
C++20 compiler). This verifies the movement maths, not Unreal collision,
input-device behaviour, rendering or performance.

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
