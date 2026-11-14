# Data

Source of truth for all tuning values (creature stats, weapon damage, oxygen
rates, upgrade costs, spawn tables). Files here are plain CSV/JSON so they can
be reviewed and changed without opening the editor; they are imported into
DataTables by the editor tooling.

| File | DataTable | Row struct | Units and meaning |
|---|---|---|---|
| `SwimTuning.csv` | `DT_SwimTuning` | `FPaleotrenchSwimSettings` | [M1 checklist](../Docs/Playtests/M1.md#adjust-feel) |
| `OxygenTuning.csv` | `DT_OxygenTuning` | `FPaleotrenchOxygenSettings` | [M2 checklist](../Docs/Playtests/M2.md#adjust-the-numbers) |
| `PowerTuning.csv` | `DT_PowerTuning` | `FPaleotrenchPowerSettings` | [M2 checklist](../Docs/Playtests/M2.md#adjust-the-numbers) |
| `LampLevels.csv` | `DT_LampLevels` | `FPaleotrenchLampLevelSettings` | one row per lamp level |
| `CaveTuning.csv` | `DT_CaveTuning` | `FPaleotrenchCaveSettings` | one row per boss cave: [M3 checklist](../Docs/Playtests/M3.md#adjust-the-numbers) |

`Tools/Prepare.bat` validates every file (ranges and cross-checks such as
critical below reserve) before importing any of them into `/Game/Data/`.
CSV-only changes need a reimport, not a C++ rebuild; with the editor open,
`python Tools/ue_live.py tables` does the same import live. Runtime validation
falls back to safe defaults with a log warning if a table is missing or invalid.
