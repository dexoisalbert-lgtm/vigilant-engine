# Tools

| File | What it does | Who runs it |
|---|---|---|
| `Build.bat` | Compiles the editor build of the game. | You, after every `git pull` that changed `Source/`. |
| `Prepare.bat` | Imports every `Data/*.csv` table, creates generated materials and prepares the test tank map (air pocket, spare tanks) and the generated cave map M3_Cave. | You, after a build that adds content and after CSV changes, with Unreal closed. |
| `Editor/prepare.py` | Validates tuning and generates the assets using Unreal Python. `--tables-only` just reimports tables. | Called by `Prepare.bat` and `ue_live.py tables`. |
| `CollectLogs.bat` | Puts build errors and the game log summary on your clipboard. | You, whenever something fails. |
| `FindEngine.bat` | Locates your Unreal install. Used by the other scripts. | — |
| `RunCoreTests.bat` | Builds and runs the engine-free core tests with the Visual Studio compiler (no CMake, no Unreal). | Before every code delivery. |
| `run_core_tests.sh` | The same tests through CMake, for machines that have it. | Optional. |
| `CoreTests/` | The tests themselves. | — |
| `CaveGen.bat` | Builds and runs `cavegen`: prints the layout of any cave seed (`--cave 1-4` for a save seed's cave as the game builds it, `--svg file` for a drawing, `--survey n` for statistics), using `Data/CaveTuning.csv`. | When a cave from a bug report needs a look. |
| `Art/decimate_static.py`, `Art/decimate_skinned.py` | Blender (run headless with `-b --python`): turn multi-million-triangle Meshy exports into game-ready meshes by decimating and re-baking all four maps. Arguments are in each file's header. | Whenever new Meshy gear or characters arrive. |
| `Art/compare_render.py` | Renders the original beside the reduced mesh, to check nothing was lost. | After a decimation. |
| `Art/ue_import_static.py` | Imports the reduced pieces into `/Game/Art/Diver/Gear` (or `GEAR_ROOT`, e.g. `/Game/Art/Weapons`) with the shared master material (run with `ue_live.py file` or headless). | After a decimation. |
| `Art/rig_fingers.py` | Blender: adds 15 finger bones per hand to the decimated diver (fitted to the fingers in the mesh) and re-weights the hands by distance along each finger. | After decimating a new player model. |
| `Art/export_finger_rig.py`, `Art/pose_lib.py` | Blender: exports the finger-rigged body, a left-arm-only mesh for first person and the one-frame arm pose as FBX. Finger poses live in `pose_lib.py`. | After `rig_fingers.py`. |
| `Art/check_finger_pose.py` | Renders a hand curled in a pose (relaxed, fist) to check the finger weights. | After rigging. |
| `Art/ue_import_fingers.py` | Imports the three FBX files onto the existing diver skeleton (new bones merge in). | After the export. |
| `Art/hand_pipeline.py` | Blender: turns a rigged first-person hand (a Meshy arm with a hand-made armature, placed in front of a scene camera) into a game-ready mesh in the rig's own space, with readable bone names and re-baked maps, and records where it sat in front of the camera. | When the hand models or rigs change in Blender. |
| `Art/hand_placement.py` | Turns the two recorded placements into the camera-relative transforms for `HandPlacement` in `PaleotrenchDiver.cpp`. Moving the hands in Blender only needs this step, not a new bake. | When the hands are moved or resized in Blender. |
| `Art/check_hands_render.py` | Renders the exported hands through a camera set up like the game camera, to compare with the Blender view. | After `hand_pipeline.py`. |
| `Art/ue_import_hands.py` | Imports the two hand meshes, their maps and material instances into `/Game/Art/Diver/Hands`. | After the pipeline. |
| `Art/weapon_pipeline.py` | Blender: makes a game-ready static mesh from one object in a scene (decimate, fresh UVs, re-baked maps), keeping its own orientation, scaled to a real size; records its scene placement. | When new weapons or props are added in Blender. |

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
