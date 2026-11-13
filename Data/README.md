# Data

Source of truth for all tuning values (creature stats, weapon damage, oxygen
rates, upgrade costs, spawn tables). Files here are plain CSV/JSON so they can
be reviewed and changed without opening the editor; they are imported into
DataTables by the editor tooling.

M1 introduces `SwimTuning.csv`. `Tools/PrepareM1.bat` validates the Default row
and imports it as `/Game/Data/DT_SwimTuning` using `FPaleotrenchSwimSettings`.
The diver loads that row at BeginPlay. CSV-only changes need a reimport, not a
C++ rebuild. See [the M1 checklist](../Docs/Playtests/M1.md#adjust-feel) for units
and settings. Runtime validation falls back to safe defaults with a log warning
if the table is missing or invalid.
