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
| `Art/decimate_static.py`, `Art/decimate_skinned.py` | Blender (run headless with `-b --python`): turn multi-million-triangle Meshy exports into game-ready meshes by decimating and re-baking all four maps. Arguments are in each file's header. | Whenever new Meshy gear or characters arrive. |
| `Art/compare_render.py` | Renders the original beside the reduced mesh, to check nothing was lost. | After a decimation. |
| `Art/ue_import_static.py` | Imports the reduced pieces into `/Game/Art/Diver/Gear` with a shared master material (run with `ue_live.py file`). | After a decimation. |
| `Art/rig_fingers.py` | Blender: adds 15 finger bones per hand to the decimated diver (fitted to the fingers in the mesh) and re-weights the hands by distance along each finger. | After decimating a new player model. |
| `Art/export_finger_rig.py`, `Art/pose_lib.py` | Blender: exports the finger-rigged body, a left-arm-only mesh for first person and the one-frame arm pose as FBX. Finger poses live in `pose_lib.py`. | After `rig_fingers.py`. |
| `Art/check_finger_pose.py` | Renders a hand curled in a pose (relaxed, fist) to check the finger weights. | After rigging. |
| `Art/ue_import_fingers.py` | Imports the three FBX files onto the existing diver skeleton (new bones merge in). | After the export. |

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
