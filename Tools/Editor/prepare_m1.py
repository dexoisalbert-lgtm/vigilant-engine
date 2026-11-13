"""Run through PrepareM1.bat after compiling the project, with the editor closed.

Creates the M1 tank and imports CSV tuning. Re-running updates the tuning table
and reuses the existing map/actors, preserving any hand-placed additions.
"""
from pathlib import Path
import csv
import math

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
MAP = "/Game/Maps/M1_TestTank"
TABLE = "/Game/Data/DT_SwimTuning"
MARKER = PROJECT / "Saved" / "M1Ready.txt"


def require(result, message):
    if not result:
        raise RuntimeError(message)
    return result


def validate_csv(path):
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    require(len(rows) == 1 and rows[0].get("Name") == "Default",
            "SwimTuning.csv must contain exactly one Default row")
    row = rows[0]
    limits = {
        "CruiseSpeed": (0.01, 2000), "SprintSpeed": (0.01, 4000),
        "DragPerSecond": (0.01, 20), "BuoyancyAcceleration": (-100, 100),
        "MouseSensitivity": (0.001, 5), "GamepadTurnRate": (0.01, 360),
        "RollRate": (0, 180), "SprintTurnScale": (0.001, 1),
        "FieldOfView": (60, 120), "FinKickInterval": (0.1, 5),
    }
    require(set(row) == {"Name", "bInvertLookY", *limits}, "Unexpected CSV columns")
    for name, (low, high) in limits.items():
        value = float(row[name])
        require(math.isfinite(value) and low <= value <= high,
                f"{name} must be between {low} and {high}")
    require(float(row["SprintSpeed"]) >= float(row["CruiseSpeed"]),
            "SprintSpeed must be at least CruiseSpeed")
    require(row["bInvertLookY"].lower() in ("true", "false"), "bInvertLookY must be True or False")


def prepare():
    MARKER.unlink(missing_ok=True)
    csv_path = PROJECT / "Data" / "SwimTuning.csv"
    validate_csv(csv_path)
    row_struct = require(unreal.load_object(None, "/Script/Paleotrench.PaleotrenchSwimSettings"),
                         "Swim settings struct is missing. Run Build.bat first.")
    tank_class = require(unreal.load_class(None, "/Script/Paleotrench.PaleotrenchTestTank"),
                         "Tank class is missing. Run Build.bat first.")
    assets = unreal.EditorAssetLibrary
    assets.make_directory("/Game/Data")
    assets.make_directory("/Game/Maps")
    table = assets.load_asset(TABLE) if assets.does_asset_exist(TABLE) else None
    if table is None:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", row_struct)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DT_SwimTuning", "/Game/Data", unreal.DataTable, factory)
    require(isinstance(table, unreal.DataTable), "DT_SwimTuning is not a DataTable")
    require(unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, str(csv_path), row_struct),
            "Could not import SwimTuning.csv")
    require(unreal.DataTableFunctionLibrary.does_data_table_row_exist(table, "Default"),
            "The imported table is missing its Default row")
    require(assets.save_loaded_asset(table), "Could not save DT_SwimTuning")

    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if assets.does_asset_exist(MAP):
        require(levels.load_level(MAP), "Could not load the existing M1 tank")
    else:
        require(levels.new_level(MAP), "Could not create the M1 tank")
    existing = actors.get_all_level_actors()
    if not any(actor.get_class() == tank_class for actor in existing):
        tank = require(actors.spawn_actor_from_class(tank_class, unreal.Vector(0, 0, 0)),
                       "Could not spawn the test tank")
        tank.set_actor_label("M1 Test Tank")
    if not any(isinstance(actor, unreal.PlayerStart) for actor in existing):
        start = require(actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-1400, 0, 450)),
                        "Could not spawn PlayerStart")
        start.set_actor_label("M1 Diver Start")
    require(levels.save_current_level(), "Could not save the tank map")
    MARKER.parent.mkdir(parents=True, exist_ok=True)
    MARKER.write_text("M1 tank and swimming tuning prepared successfully.\n", encoding="utf-8")
    unreal.log("PALEOTRENCH M1 PREPARATION SUCCEEDED")


if __name__ == "__main__":
    prepare()
