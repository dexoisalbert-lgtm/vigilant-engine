"""Prepares generated project content. Run through Tools/Prepare.bat after
compiling, with the editor closed. Repeatable: tuning is reimported and
existing assets and actors are reused, so hand-made additions survive.

  --tables-only   only validate and reimport the CSV tuning tables. Safe in an
                  open editor; Tools/ue_live.py tables uses it.
"""
from pathlib import Path
import csv
import math
import sys

import unreal


PROJECT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
DATA = PROJECT / "Data"
MAP = "/Game/Maps/M1_TestTank"
WATER_MATERIAL = "/Game/Materials/M_WaterSurface"
MARKER = PROJECT / "Saved" / "Prepared.txt"

# The test tank's air dome (Source/Paleotrench/Private/World/PaleotrenchTestTank.cpp).
AIR_POCKET_ID = "TankDome"
AIR_POCKET_AT = (-1300.0, 0.0, 1825.0)
SPARE_TANKS = {
    "SpareTank_Gate": (1400.0, 0.0, 1100.0),     # inside the high gate
    "SpareTank_Floor": (1700.0, -1200.0, 120.0),  # far corner, near the floor
}


def require(result, message):
    if not result:
        raise RuntimeError(message)
    return result


def number(low, high):
    return ("number", low, high)


def integer(low, high):
    return ("int", low, high)


BOOL = ("bool",)

# Ranges mirror the IsValid() checks in Source/Paleotrench/Public/Player/*Settings.h,
# so a bad edit is rejected here before any asset changes.
TABLES = [
    {
        "csv": "SwimTuning.csv",
        "asset": "DT_SwimTuning",
        "struct": "/Script/Paleotrench.PaleotrenchSwimSettings",
        "rows": ["Default"],
        "columns": {
            "CruiseSpeed": number(0.01, 2000), "SprintSpeed": number(0.01, 4000),
            "DragPerSecond": number(0.01, 20), "BuoyancyAcceleration": number(-100, 100),
            "MouseSensitivity": number(0.001, 5), "GamepadTurnRate": number(0.01, 360),
            "RollRate": number(0, 180), "SprintTurnScale": number(0.001, 1),
            "FieldOfView": number(60, 120), "bInvertLookY": BOOL,
            "FinKickInterval": number(0.1, 5), "BusySpeedScale": number(0.05, 1),
        },
        "checks": [
            (lambda r: r["SprintSpeed"] >= r["CruiseSpeed"], "SprintSpeed must be at least CruiseSpeed"),
        ],
    },
    {
        "csv": "OxygenTuning.csv",
        "asset": "DT_OxygenTuning",
        "struct": "/Script/Paleotrench.PaleotrenchOxygenSettings",
        "rows": ["Default"],
        "columns": {
            "TankCapacityBar": number(1, 1000), "RestBarPerMinute": number(0, 1000),
            "SwimBarPerMinute": number(0, 1000), "SprintBarPerMinute": number(0, 1000),
            "ExertionBarPerMinute": number(0, 1000), "ReserveBar": number(0, 1000),
            "CriticalBar": number(0.01, 1000), "AirRefillBarPerSecond": number(0.01, 1000),
            "SwapSeconds": number(0.1, 60), "StartSpareTanks": integer(0, 10),
            "MaxSpareTanks": integer(0, 10), "EmptyGraceSeconds": number(0, 60),
            "SlowestSwimScale": number(0.05, 1),
        },
        "checks": [
            (lambda r: r["ReserveBar"] <= r["TankCapacityBar"], "ReserveBar must not exceed TankCapacityBar"),
            (lambda r: r["CriticalBar"] <= r["ReserveBar"], "CriticalBar must not exceed ReserveBar"),
            (lambda r: r["StartSpareTanks"] <= r["MaxSpareTanks"], "StartSpareTanks must not exceed MaxSpareTanks"),
        ],
    },
    {
        "csv": "PowerTuning.csv",
        "asset": "DT_PowerTuning",
        "struct": "/Script/Paleotrench.PaleotrenchPowerSettings",
        "rows": ["Default"],
        "columns": {
            "PackCapacity": number(0.1, 100000), "StartPackFraction": number(0, 1),
            "LampCellCapacity": number(0.1, 100000), "StartLampFraction": number(0, 1),
            "ChargeRatePerSecond": number(0, 100000), "ChargeEfficiency": number(0.01, 1),
            "CrankRatePerSecond": number(0, 100000), "CellLeakPerSecond": number(0, 1000),
            "LowCellFraction": number(0, 1), "BurstSeconds": number(0.1, 10),
        },
        "checks": [],
    },
    {
        "csv": "LampLevels.csv",
        "asset": "DT_LampLevels",
        "struct": "/Script/Paleotrench.PaleotrenchLampLevelSettings",
        "rows": ["Off", "Low", "Medium", "High", "Burst"],
        "columns": {
            "DrainPerSecond": number(0, 1000), "IntensityLumens": number(0, 100000),
            "AttenuationRadius": number(10, 20000), "InnerConeAngle": number(0, 80),
            "OuterConeAngle": number(0, 80),
        },
        "checks": [
            (lambda r: r["OuterConeAngle"] >= r["InnerConeAngle"], "OuterConeAngle must be at least InnerConeAngle"),
        ],
    },
]


def parse_value(table, row_name, column, text, kind):
    where = f"{table}, row {row_name}, column {column}"
    if kind[0] == "bool":
        require(text.lower() in ("true", "false"), f"{where}: must be True or False")
        return text.lower() == "true"
    if kind[0] == "int":
        require(text.strip().lstrip("-").isdigit(), f"{where}: must be a whole number")
        value = int(text)
    else:
        try:
            value = float(text)
        except ValueError:
            raise RuntimeError(f"{where}: '{text}' is not a number")
        require(math.isfinite(value), f"{where}: must be finite")
    require(kind[1] <= value <= kind[2], f"{where}: must be between {kind[1]} and {kind[2]}")
    return value


def validate_table(spec):
    path = DATA / spec["csv"]
    require(path.exists(), f"Missing {path}")
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    names = [row.get("Name") for row in rows]
    require(names == spec["rows"], f"{spec['csv']}: rows must be exactly {spec['rows']}, found {names}")
    expected = {"Name", *spec["columns"]}
    for row in rows:
        require(set(row) == expected, f"{spec['csv']}: columns must be {sorted(expected)}")
        require(all(value is not None for value in row.values()),
                f"{spec['csv']}, row {row['Name']}: has fewer values than there are columns")
        values = {column: parse_value(spec["csv"], row["Name"], column, row[column], kind)
                  for column, kind in spec["columns"].items()}
        for check, message in spec["checks"]:
            require(check(values), f"{spec['csv']}, row {row['Name']}: {message}")
    return path


def import_table(spec, csv_path):
    assets = unreal.EditorAssetLibrary
    struct = require(unreal.load_object(None, spec["struct"]),
                     f"{spec['struct']} is missing. Run Build.bat first.")
    path = f"/Game/Data/{spec['asset']}"
    assets.make_directory("/Game/Data")
    table = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if table is None:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", struct)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            spec["asset"], "/Game/Data", unreal.DataTable, factory)
    require(isinstance(table, unreal.DataTable), f"{spec['asset']} is not a DataTable")
    require(unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, str(csv_path), struct),
            f"Could not import {spec['csv']}")
    for row in spec["rows"]:
        require(unreal.DataTableFunctionLibrary.does_data_table_row_exist(table, row),
                f"{spec['asset']} is missing its {row} row after import")
    require(assets.save_loaded_asset(table), f"Could not save {spec['asset']}")


def prepare_tables():
    # Validate everything first, so one bad file changes no assets at all.
    paths = [validate_table(spec) for spec in TABLES]
    for spec, csv_path in zip(TABLES, paths):
        import_table(spec, csv_path)
    unreal.log(f"PALEOTRENCH TABLES IMPORTED: {', '.join(spec['csv'] for spec in TABLES)}")


def ensure_water_material():
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(WATER_MATERIAL):
        return
    assets.make_directory("/Game/Materials")
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_WaterSurface", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    require(material, "Could not create M_WaterSurface")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    library = unreal.MaterialEditingLibrary
    colour = library.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    colour.set_editor_property("constant", unreal.LinearColor(0.32, 0.58, 0.64, 1.0))
    opacity = library.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    opacity.set_editor_property("r", 0.4)
    require(library.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR),
            "Could not wire the water colour")
    require(library.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY),
            "Could not wire the water opacity")
    library.recompile_material(material)
    require(assets.save_loaded_asset(material), "Could not save M_WaterSurface")


def prepare_map():
    tank_class = require(unreal.load_class(None, "/Script/Paleotrench.PaleotrenchTestTank"),
                         "Tank class is missing. Run Build.bat first.")
    pocket_class = require(unreal.load_class(None, "/Script/Paleotrench.PaleotrenchAirPocket"),
                           "Air pocket class is missing. Run Build.bat first.")
    spare_class = require(unreal.load_class(None, "/Script/Paleotrench.PaleotrenchSpareTank"),
                          "Spare tank class is missing. Run Build.bat first.")
    unreal.EditorAssetLibrary.make_directory("/Game/Maps")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        require(levels.load_level(MAP), "Could not load the test tank map")
    else:
        require(levels.new_level(MAP), "Could not create the test tank map")
    existing = actors.get_all_level_actors()

    def spawn(cls, location, label):
        actor = require(actors.spawn_actor_from_class(cls, unreal.Vector(*location)), f"Could not spawn {label}")
        actor.set_actor_label(label)
        return actor

    if not any(actor.get_class() == tank_class for actor in existing):
        spawn(tank_class, (0, 0, 0), "Test Tank")
    if not any(isinstance(actor, unreal.PlayerStart) for actor in existing):
        spawn(unreal.PlayerStart, (-1400, 0, 450), "Diver Start")

    pockets = [a for a in existing if a.get_class() == pocket_class]
    if not any(str(a.get_editor_property("checkpoint_id")) == AIR_POCKET_ID for a in pockets):
        pocket = spawn(pocket_class, AIR_POCKET_AT, "Air Pocket (Tank Dome)")
        pocket.set_editor_property("checkpoint_id", AIR_POCKET_ID)

    spare_ids = {str(a.get_editor_property("pickup_id")) for a in existing if a.get_class() == spare_class}
    for pickup_id, location in SPARE_TANKS.items():
        if pickup_id not in spare_ids:
            tank = spawn(spare_class, location, pickup_id.replace("_", " "))
            tank.set_editor_property("pickup_id", pickup_id)

    require(levels.save_current_level(), "Could not save the test tank map")


def prepare(tables_only):
    if not tables_only:
        MARKER.unlink(missing_ok=True)
    prepare_tables()
    if tables_only:
        return
    ensure_water_material()
    prepare_map()
    MARKER.parent.mkdir(parents=True, exist_ok=True)
    MARKER.write_text("Tables, materials and the test tank map prepared successfully.\n", encoding="utf-8")
    unreal.log("PALEOTRENCH PREPARATION SUCCEEDED")


if __name__ == "__main__":
    prepare("--tables-only" in sys.argv[1:])
