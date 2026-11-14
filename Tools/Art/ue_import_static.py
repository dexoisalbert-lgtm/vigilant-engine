import os, unreal
# Folder written by decimate_static.py or weapon_pipeline.py (one sub-folder per piece); override with
# GEAR_DECIMATED. GEAR_ROOT picks the content folder (weapons go to /Game/Art/Weapons). Every piece uses the
# shared master material in /Game/Art/Diver/Gear.
SRC = os.environ.get("GEAR_DECIMATED", os.path.join(os.path.expanduser("~"), "Downloads", "GearForPlayer", "Decimated"))
ROOT = os.environ.get("GEAR_ROOT", "/Game/Art/Diver/Gear")
MASTER_ROOT = "/Game/Art/Diver/Gear"
at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

def imp(files, dest, opts=None):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f; t.destination_path = dest; t.automated = True; t.replace_existing = True; t.save = True
        if opts: t.options = opts
        tasks.append(t)
    at.import_asset_tasks(tasks)
    return [p for t in tasks for p in t.imported_object_paths]

def tex_setup(path, kind):
    t = unreal.load_asset(path)
    if kind == "BC":
        t.set_editor_property("srgb", True); t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif kind == "N":
        t.set_editor_property("srgb", False); t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property("flip_green_channel", True)  # Blender bakes OpenGL normals
    else:
        t.set_editor_property("srgb", False); t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    eal.save_asset(path)
    return t

pieces = ONLY if "ONLY" in globals() else sorted(d for d in os.listdir(SRC) if os.path.isfile(os.path.join(SRC, d, "SM_%s.fbx" % d)))
master_path = MASTER_ROOT + "/M_GearMaster"
master = None
for name in pieces:
    folder = os.path.join(SRC, name); dest = "%s/%s" % (ROOT, name)
    texs = {}
    for k in ("BC", "N", "M", "R"):
        imp([os.path.join(folder, "T_%s_%s.png" % (name, k))], dest)
        texs[k] = tex_setup("%s/T_%s_%s" % (dest, name, k), k)
    if master is None:
        if eal.does_asset_exist(master_path):
            master = unreal.load_asset(master_path)
        else:
            master = at.create_asset("M_GearMaster", MASTER_ROOT, unreal.Material, unreal.MaterialFactoryNew())
            spec = [("BaseColor", "BC", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, unreal.MaterialProperty.MP_BASE_COLOR, -400),
                    ("Metallic", "M", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, unreal.MaterialProperty.MP_METALLIC, -150),
                    ("Roughness", "R", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, unreal.MaterialProperty.MP_ROUGHNESS, 100),
                    ("Normal", "N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, unreal.MaterialProperty.MP_NORMAL, 350)]
            for pname, k, st, prop, y in spec:
                n = mel.create_material_expression(master, unreal.MaterialExpressionTextureSampleParameter2D, -400, y)
                n.set_editor_property("parameter_name", pname)
                n.set_editor_property("sampler_type", st)
                n.set_editor_property("texture", texs[k])
                mel.connect_material_property(n, "RGB" if k in ("BC", "N") else "R", prop)
            mel.recompile_material(master)
            eal.save_asset(master_path)
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True); opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False); opts.set_editor_property("import_textures", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = opts.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True); sm.set_editor_property("auto_generate_collision", True)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    imp([os.path.join(folder, "SM_%s.fbx" % name)], dest, opts)
    mi_path = "%s/MI_%s" % (dest, name)
    mi = unreal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else \
        at.create_asset("MI_" + name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    for pname, k in (("BaseColor", "BC"), ("Metallic", "M"), ("Roughness", "R"), ("Normal", "N")):
        mel.set_material_instance_texture_parameter_value(mi, pname, texs[k])
    mel.update_material_instance(mi); eal.save_asset(mi_path)
    mesh = unreal.load_asset("%s/SM_%s" % (dest, name))
    mesh.set_material(0, mi)
    eal.save_asset(mesh.get_path_name())
    b = mesh.get_bounding_box()
    print("GEAR %s tris=%d size_cm=%s" % (name, mesh.get_num_triangles(0), (b.max - b.min)))
print("GEAR_IMPORT_DONE", len(pieces))
