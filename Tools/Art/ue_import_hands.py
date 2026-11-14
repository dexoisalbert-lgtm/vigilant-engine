import os, unreal
# Imports the camera-space first-person hands made by the hand pipeline script (SK_FPHand_L/R + four maps each) into
# /Game/Art/Diver/Hands, with a material instance of the shared M_GearMaster per hand. Run with the ue_live file command.
# Do not write a file extension in this text: the remote runner mistakes it for a script path.
# HANDS_DIR overrides the folder (default: Downloads/GearForPlayer/Hands/Decimated).
SRC = os.environ.get("HANDS_DIR", os.path.join(os.path.expanduser("~"), "Downloads", "GearForPlayer", "Hands", "Decimated"))
DEST = "/Game/Art/Diver/Hands"
at = unreal.AssetToolsHelpers.get_asset_tools(); eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
master = unreal.load_asset("/Game/Art/Diver/Gear/M_GearMaster")

def run(files, opts=None, dest=DEST):
    ts = []
    for f in files:
        t = unreal.AssetImportTask(); t.filename = f; t.destination_path = dest; t.automated = True
        t.replace_existing = True; t.save = True
        if opts: t.options = opts
        ts.append(t)
    at.import_asset_tasks(ts)
    return [p for t in ts for p in t.imported_object_paths]

for side in ("L", "R"):
    name = "FPHand_" + side
    texs = {}
    for k in ("BC", "N", "M", "R"):
        run([os.path.join(SRC, "T_%s_%s.png" % (name, k))])
        p = "%s/T_%s_%s" % (DEST, name, k); t = unreal.load_asset(p)
        if k == "BC": t.set_editor_property("srgb", True)
        elif k == "N":
            t.set_editor_property("srgb", False); t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("flip_green_channel", True)
        else:
            t.set_editor_property("srgb", False); t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        eal.save_asset(p); texs[k] = t
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True); o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property("import_materials", False); o.set_editor_property("import_textures", False)
    o.set_editor_property("import_animations", False); o.set_editor_property("create_physics_asset", False)
    sk = o.skeletal_mesh_import_data
    sk.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    sk.set_editor_property("import_morph_targets", False)
    # Each hand is imported into its own folder first: Unreal reuses a skeleton it finds in the target folder, and the
    # two hands have different bones, so the second one would end up with an incompatible skeleton.
    tmp = DEST + "/_import_" + side
    paths = []
    for p in run([os.path.join(SRC, "SK_%s.fbx" % name)], o, tmp):
        target = "%s/%s" % (DEST, p.split("/")[-1].split(".")[0])
        if eal.does_asset_exist(target): eal.delete_asset(target)
        eal.rename_asset(p.split(".")[0], target)
        paths.append(target + "." + target.split("/")[-1])
    eal.delete_directory(tmp)
    print("HANDS imported", paths)
    mi_path = "%s/MI_%s" % (DEST, name)
    mi = unreal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else at.create_asset("MI_" + name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    for pn, k in (("BaseColor", "BC"), ("Metallic", "M"), ("Roughness", "R"), ("Normal", "N")):
        mel.set_material_instance_texture_parameter_value(mi, pn, texs[k])
    mel.update_material_instance(mi); eal.save_asset(mi_path)
    for p in paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SkeletalMesh):
            # the materials list holds struct copies: build a new list (editing the copies does nothing)
            mats = [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=m.material_slot_name, uv_channel_data=m.uv_channel_data)
                    for m in a.get_editor_property("materials")]
            a.set_editor_property("materials", mats); eal.save_asset(a.get_path_name())
            b = a.get_bounds()
            print("HANDS mesh", a.get_name(), "origin", b.origin, "extent", b.box_extent, "skeleton", a.get_editor_property("skeleton").get_name())
print("HANDS done")
