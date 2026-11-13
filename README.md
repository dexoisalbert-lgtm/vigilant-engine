# Paleotrench

First-person underwater horror in Unreal Engine 5. A scuba diver is pulled
through a rift into a prehistoric cave system and must kill four ancient
predators and repair a time machine before the oxygen runs out.

- **Design and roadmap:** [Docs/GDD.md](Docs/GDD.md)
- **Setting up your PC:** [Docs/SETUP.md](Docs/SETUP.md)
- **Current milestone checklist:** [Docs/Playtests/M2.md](Docs/Playtests/M2.md)

## Status

| Milestone | State |
|---|---|
| M0 — project skeleton, build scripts, core test harness | done; compiles on Unreal 5.7 |
| M1 — swimming, camera, KB/M + gamepad, underwater test tank | done; playtested and tuned (faster swim, head-like camera, lean/peek, lamp lag) |
| M2 — oxygen, battery pack, lamp levels, charging, wrist computer, save/load | implemented; 59 core tests pass; builds and runs on Unreal 5.7; your playtest pending |
| M3 — procedural caves | next |

## Try it

Use branch **`ccr-c50880a2-g0siyj`**, with Unreal **5.7**. Close the editor,
run `Tools/Build.bat`, then `Tools/Prepare.bat`. Open `Paleotrench.uproject`
and press **Play**. The preparation step creates the tank map, the tuning
DataTables and the water material; no manually wired Blueprints or downloaded
models are required.

M2 turns the tank into a survival timer: a 200 bar tank with spare tanks, an
air pocket in the ceiling that refills you and saves a checkpoint, a battery
pack that charges a head lamp with Off/Low/Medium/High and a burst strobe
(with a hand crank as a last resort), a wrist dive computer, mask-edge warning
LEDs, low-air tunnel vision, blackout and respawn. Controls and checks:
[M2.md](Docs/Playtests/M2.md).

The diver has camera-relative movement in all three axes, quaternion
pitch/yaw/roll, inertia, drag, sprinting and swept collision. The test tank
contains pillars, a slope, elevated gates and depth markers. A fixed-power
headlamp, blue-green fog and colour grading establish the first underwater
look. Battery costs, walking in air pockets, audio and final water VFX come
in later milestones.

Swimming controls are in [M1.md](Docs/Playtests/M1.md). All tuning lives in
`Data/*.csv`; rerun `Prepare.bat` after editing (or reimport live with
`python Tools/ue_live.py tables` while the editor is open).

Engine-free verification: `Tools\RunCoreTests.bat` (uses the Visual Studio
compiler Unreal already needs) or `bash Tools/run_core_tests.sh` (CMake). This
verifies the movement, oxygen, power and save-file maths, not Unreal collision,
input-device behaviour, rendering or performance.

## Layout

```
Paleotrench.uproject   open this in Unreal
Source/PaleotrenchCore engine-free logic (random, seeds, swimming, oxygen, power, save files; later caves)
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
