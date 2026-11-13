# Tools

| File | What it does | Who runs it |
|---|---|---|
| `Build.bat` | Compiles the editor build of the game. | You, after every `git pull` that changed `Source/`. |
| `Prepare.bat` | Imports every `Data/*.csv` table, creates generated materials and prepares the test tank map (air pocket, spare tanks). | You, after a build that adds content and after CSV changes, with Unreal closed. |
| `Editor/prepare.py` | Validates tuning and generates the assets using Unreal Python. `--tables-only` just reimports tables. | Called by `Prepare.bat` and `ue_live.py tables`. |
| `CollectLogs.bat` | Puts build errors and the game log summary on your clipboard. | You, whenever something fails. |
| `FindEngine.bat` | Locates your Unreal install. Used by the other scripts. | — |
| `RunCoreTests.bat` | Builds and runs the engine-free core tests with the Visual Studio compiler (no CMake, no Unreal). | Before every code delivery. |
| `run_core_tests.sh` | The same tests through CMake, for machines that have it. | Optional. |
| `CoreTests/` | The tests themselves. | — |

The preparation log is `Saved/Logs/Prepare.txt`; `CollectLogs.bat` includes it.
The success marker (`Saved/Prepared.txt`) is written only after the tables,
materials and map have all been saved. Generated `.umap`/`.uasset` files use
Git LFS and are not committed yet.

## Live editor control (`ue_live.py`)

Drives the open editor through Unreal's built-in Python remote execution
(local machine only), so most changes no longer need a close/reopen:
`ping`, `livecode` (Live Coding compile of `.cpp` changes), `tables`
(validate and reimport every `Data/*.csv`), `play`, `stop`, `exec`, `file`.
Header changes (new members, UPROPERTYs, classes) still need `Build.bat` with
the editor closed.
