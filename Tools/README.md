# Tools

| File | What it does | Who runs it |
|---|---|---|
| `Build.bat` | Compiles the editor build of the game. | You, after every `git pull` that changed `Source/`. |
| `CollectLogs.bat` | Puts build errors and the game log summary on your clipboard. | You, whenever something fails. |
| `FindEngine.bat` | Locates your Unreal install. Used by the other scripts. | — |
| `run_core_tests.sh` | Builds and runs the engine-free core tests (no Unreal needed). | Claude, before every commit. |
| `CoreTests/` | The tests themselves. | — |
