# usage: blender -b <scene.blend> --python weapon_pipeline.py -- <object> <out_dir> <name> <target_tris> <real_size_m> [bake_res] [voxel]
# Makes a game-ready static mesh from one high-poly (Meshy) object inside a Blender scene, the way decimate_static.py
# does for an FBX: decimate without UVs, fresh smart UVs, then re-bake base colour, metallic, roughness and a tangent
# normal map from the original. Differences: the source is an object in a scene, and the result keeps the object's
# OWN orientation (the rotation and scale it was posed with in the scene are not applied), centred on its bounding
# box and scaled so its longest side is <real_size_m>. The scene placement and the centring offset are written to
# <name>_placement.json, so the object can be put back where it was (for example in the first-person view).
# voxel: optional voxel-remesh size as a fraction of the longest side, for meshes that shred when collapsed.
import bpy, bmesh, sys, os, time, math, json
from mathutils import Vector, Matrix

a = sys.argv[sys.argv.index("--") + 1:]
obj_name, out_dir, name, target, real = a[0], a[1], a[2], int(a[3]), float(a[4])
res = int(a[5]) if len(a) > 5 else 2048
VOXEL = float(a[6]) if len(a) > 6 else 0.0
os.makedirs(out_dir, exist_ok=True)
T0 = time.time()
def log(*x): print("[weapon]", name, *x, "t=%.0fs" % (time.time() - T0), flush=True)

sc = bpy.context.scene
cam = sc.camera
placement = {"source_object": obj_name, "scene_file": bpy.data.filepath,
             "world_matrix": [list(r) for r in bpy.data.objects[obj_name].matrix_world],
             "camera_matrix": [list(r) for r in cam.matrix_world] if cam else None,
             "camera_lens_mm": cam.data.lens if cam else None, "camera_sensor_mm": cam.data.sensor_width if cam else None}
for o in list(sc.objects):
    if o.name != obj_name: bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
hi = bpy.data.objects[obj_name]
hi.name = name + "_high"
hi.parent = None
hi.matrix_world = Matrix.Identity(4)            # the mesh's own frame, not the pose in the scene
bb = [Vector(c) for c in hi.bound_box]
centre = sum(bb, Vector()) / 8
hi.data.transform(Matrix.Translation(-centre))
dims = hi.dimensions.copy()
s = real / max(dims)
placement.update({"local_bbox_centre": list(centre), "local_dims": list(dims), "scale_to_real": s, "real_size_m": real})
src_tris = sum(len(p.vertices) - 2 for p in hi.data.polygons)
log("source", src_tris, "tris; local dims", [round(d, 3) for d in dims], "scale", round(s, 4))

lo = hi.copy(); lo.data = hi.data.copy(); lo.name = name
sc.collection.objects.link(lo)
while lo.data.uv_layers: lo.data.uv_layers.remove(lo.data.uv_layers[0])
lo.data.materials.clear()
bm = bmesh.new(); bm.from_mesh(lo.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=max(dims) * 1e-6)
bm.to_mesh(lo.data); bm.free()
for o in sc.objects: o.select_set(o == lo)
bpy.context.view_layer.objects.active = lo
if VOXEL > 0:
    rm = lo.modifiers.new("remesh", 'REMESH'); rm.mode = 'VOXEL'
    rm.voxel_size = max(dims) * VOXEL; rm.adaptivity = 0.0
    bpy.ops.object.modifier_apply(modifier=rm.name)
    bm = bmesh.new(); bm.from_mesh(lo.data); bmesh.ops.triangulate(bm, faces=bm.faces); bm.to_mesh(lo.data); bm.free()
    log("voxel remesh", len(lo.data.polygons), "faces")
cur = len(lo.data.polygons)
while cur > target * 1.02:
    m = lo.modifiers.new("dec", 'DECIMATE'); m.decimate_type = 'COLLAPSE'
    m.ratio = max(target / cur, 0.3); m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=m.name)
    new = len(lo.data.polygons); log("decimate", cur, "->", new)
    if new >= cur * 0.98: break
    cur = new
for ob in (hi, lo):
    ob.data.transform(Matrix.Scale(s, 4)); ob.data.update()
bm = bmesh.new(); bm.from_mesh(lo.data)
bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=real * 1e-5)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(lo.data); bm.free()
lo.data.shade_smooth()
try: bpy.ops.object.shade_auto_smooth(angle=math.radians(45))
except Exception as e: log("auto smooth skipped", e)
for mod in list(lo.modifiers):
    try: bpy.ops.object.modifier_apply(modifier=mod.name)
    except Exception: pass
lo.data.uv_layers.new(name="UVMap")
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, scale_to_bounds=True)
try: bpy.ops.uv.pack_islands(rotate=True, margin=0.004)
except Exception as e: log("pack skipped", e)
bpy.ops.object.mode_set(mode='OBJECT')
log("low", len(lo.data.polygons), "tris, UVs done")

sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1
b = sc.render.bake
b.use_selected_to_active = True; b.cage_extrusion = real * 0.015; b.max_ray_distance = real * 0.04
b.margin = 16; b.margin_type = 'EXTEND'
hmat = hi.data.materials[0]; hnt = hmat.node_tree
hb = next(n for n in hnt.nodes if n.type == 'BSDF_PRINCIPLED')
hout = next(n for n in hnt.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
emit = hnt.nodes.new('ShaderNodeEmission'); emit.inputs['Strength'].default_value = 1.0
lmat = bpy.data.materials.new("M_" + name); lmat.use_nodes = True; lo.data.materials.append(lmat)
lnt = lmat.node_tree; lb = next(n for n in lnt.nodes if n.type == 'BSDF_PRINCIPLED')
for o in sc.objects: o.select_set(o in (hi, lo))
bpy.context.view_layer.objects.active = lo
def bake(suffix, kind, sock=None, srgb=False):
    img = bpy.data.images.new("T_%s_%s" % (name, suffix), res, res, alpha=False)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    t = lnt.nodes.new('ShaderNodeTexImage'); t.image = img; lnt.nodes.active = t
    if kind == 'EMIT':
        for l in list(emit.inputs['Color'].links): hnt.links.remove(l)
        if sock.links: hnt.links.new(sock.links[0].from_socket, emit.inputs['Color'])
        else:
            v = sock.default_value; emit.inputs['Color'].default_value = tuple(v) if hasattr(v, '__len__') else (v, v, v, 1)
        hnt.links.new(emit.outputs[0], hout.inputs['Surface']); bpy.ops.object.bake(type='EMIT')
        hnt.links.new(hb.outputs[0], hout.inputs['Surface'])
    else:
        bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT')
    img.filepath_raw = os.path.join(out_dir, img.name + ".png"); img.file_format = 'PNG'; img.save()
    img.filepath = img.filepath_raw; log("baked", suffix); return t
tBC = bake("BC", 'EMIT', hb.inputs['Base Color'], True)
tM = bake("M", 'EMIT', hb.inputs['Metallic'])
tR = bake("R", 'EMIT', hb.inputs['Roughness'])
tN = bake("N", 'NORMAL')
lnt.links.new(tBC.outputs[0], lb.inputs['Base Color']); lnt.links.new(tM.outputs[0], lb.inputs['Metallic'])
lnt.links.new(tR.outputs[0], lb.inputs['Roughness'])
nm = lnt.nodes.new('ShaderNodeNormalMap'); lnt.links.new(tN.outputs[0], nm.inputs['Color']); lnt.links.new(nm.outputs[0], lb.inputs['Normal'])

bpy.data.objects.remove(hi, do_unlink=True)
for o in sc.objects: o.select_set(o == lo)
bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, "SM_%s.fbx" % name), use_selection=True,
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', use_tspace=True,
                         path_mode='STRIP', axis_forward='-Y', axis_up='Z')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "%s_low.blend" % name))
placement["low_tris"] = len(lo.data.polygons)
with open(os.path.join(out_dir, "%s_placement.json" % name), "w") as f: json.dump(placement, f, indent=1)
log("DONE", src_tris, "->", len(lo.data.polygons))
