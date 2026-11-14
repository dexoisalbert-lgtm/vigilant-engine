# Imports the finger-rigged diver (SK_Diver, SK_Diver_FPArm, A_Diver_FP_LeftArm) onto the existing skeleton.
# Run with ue_live.py file. FING_STEP=body|arm|anim runs one step; FINGER_RIG_DIR is the export_finger_rig.py output.
import os, unreal
SRC = os.environ.get("FINGER_RIG_DIR", os.path.join(os.path.expanduser("~"), "Downloads", "GearForPlayer", "Decimated", "Player", "Diver", "Fingers"))
DEST = "/Game/Art/Diver/Player"
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary
skel_path = DEST + "/SK_Diver_Skeleton"
skel = unreal.load_asset(skel_path)
mi = unreal.load_asset(DEST + "/MI_Diver")

def run(fbx, opts):
    t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, fbx); t.destination_path = DEST; t.automated = True
    t.replace_existing = True; t.save = True; t.options = opts
    at.import_asset_tasks([t]); return list(t.imported_object_paths)

def mesh_opts(anim):
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True); o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property("import_materials", False); o.set_editor_property("import_textures", False)
    o.set_editor_property("import_animations", anim); o.set_editor_property("create_physics_asset", False)
    o.set_editor_property("skeleton", skel)
    sk = o.skeletal_mesh_import_data
    sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    sk.set_editor_property("import_morph_targets", False)
    return o

def set_mat(path):
    a = unreal.load_asset(path)
    mats = a.get_editor_property("materials")
    for m in mats: m.set_editor_property("material_interface", mi)
    a.set_editor_property("materials", mats); eal.save_asset(path)
    return a

which = os.environ.get("FING_STEP", "all")
if which in ("all", "body"):
    print("FING body ->", run("SK_Diver.fbx", mesh_opts(False)))
    a = set_mat(DEST + "/SK_Diver")
    print("FING SK_Diver bounds", a.get_bounds().box_extent, "skeleton", a.get_editor_property("skeleton").get_name())
if which in ("all", "arm"):
    print("FING arm ->", run("SK_Diver_FPArm.fbx", mesh_opts(False)))
    a = set_mat(DEST + "/SK_Diver_FPArm")
    print("FING SK_Diver_FPArm bounds", a.get_bounds().origin, a.get_bounds().box_extent, "skeleton", a.get_editor_property("skeleton").get_name())
if which in ("all", "anim"):
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", False); o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    o.set_editor_property("import_animations", True); o.set_editor_property("skeleton", skel)
    o.anim_sequence_import_data.set_editor_property("import_bone_tracks", True)
    paths = run("A_Diver_FP_LeftArm.fbx", o)
    print("FING anim ->", paths)
    for p in paths:
        an = unreal.load_asset(p)
        if isinstance(an, unreal.AnimSequence):
            print("FING anim", an.get_name(), "length", an.get_play_length(), "frames", an.get_number_of_sampled_keys() if hasattr(an, "get_number_of_sampled_keys") else "?")
            try: print("FING anim tracks", len(unreal.AnimationLibrary.get_animation_track_names(an)))
            except Exception as e: print("FING track count err", e)
print("FING done")
