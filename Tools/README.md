# Tools

| File | What it does | Who runs it |
|---|---|---|
| `Build.bat` | Compiles the editor build of the game. | You, after every `git pull` that changed `Source/`. |
| `PrepareM1.bat` | Creates/reuses the tank map and imports swimming CSV tuning. | You, after the first M1 build and after CSV changes, with Unreal closed. |
| `Editor/prepare_m1.py` | Validates tuning and generates the M1 assets using Unreal Python. | Called by `PrepareM1.bat`. |
| `CollectLogs.bat` | Puts build errors and the game log summary on your clipboard. | You, whenever something fails. |
| `FindEngine.bat` | Locates your Unreal install. Used by the other scripts. | — |
| `run_core_tests.sh` | Builds and runs the engine-free core tests (no Unreal needed). | Before every code delivery; requires CMake 3.16+ and a C++20 compiler. |
| `CoreTests/` | The tests themselves. | — |

The M1 preparation log is `Saved/Logs/M1-Prepare.txt`; `CollectLogs.bat`
includes it. A success marker is written only after both the imported table
and map have been saved. The generated `.umap` and `.uasset` use Git LFS.

## Live editor control (`ue_live.py`)

Drives the open editor through Unreal's built-in Python remote execution
(local machine only), so most changes no longer need a close/reopen:
`ping`, `livecode` (Live Coding compile of `.cpp` body changes), `tuning`
(reimport `Data/SwimTuning.csv`), `play`, `stop`, `exec`, `file`.
Header changes (new members, UPROPERTYs, classes) still need `Build.bat` with
the editor closed.
