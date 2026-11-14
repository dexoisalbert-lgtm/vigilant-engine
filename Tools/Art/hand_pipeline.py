# usage: blender -b <Hands.blend> --python hand_pipeline.py -- <out_dir> <Left|Right> <target_tris> <bake_res>
# Turns one of the user's rigged first-person hands (high-poly Meshy arm + hand-made armature, placed with object
# transforms in front of a scene camera) into a game-ready skinned mesh:
#   * the mesh is expressed in the ARMATURE's own space and the armature object is reset to identity, so the asset
#     matches animations exported from the same rig (bone units and orientations are the rig's own),
#   * the bones get readable names (hand, thumb/index/middle/ring/pinky chains),
#   * the mesh is decimated and its four maps are re-baked from the high-poly mesh,
#   * the result is exported as SK_FPHand_<L|R>.fbx (no animation: the rig's rest pose is the reference pose),
#   * where the hand sits in front of the camera is written to FPHand_<L|R>_placement.json; hand_placement.py turns
#     the two placements into the camera-relative transforms used in Unreal (moving the hands in Blender only needs
#     that second step, not a new bake).
import bpy, bmesh, sys, os, time, math, json
from mathutils import Matrix

a = sys.argv[sys.argv.index("--") + 1:]
out_dir, side, target, res = a[0], a[1], int(a[2]), int(a[3])
os.makedirs(out_dir, exist_ok=True)
T0 = time.time()
def log(*x): print("[hand]", *x, "t=%.0fs" % (time.time() - T0), flush=True)

PAIRS = {"Left": ("Armature.001", "Mesh_0.001"), "Right": ("Armature", "Mesh_0")}
# old bone -> new bone (read off the rigs: root = wrist bone, then index/middle/ring/pinky/thumb chains)
CHAINS = {"Left": {"index": ["Bone.001", "Bone.002", "Bone.003"], "middle": ["Bone.004", "Bone.005", "Bone.006"],
                   "ring": ["Bone.007", "Bone.008", "Bone.009"], "pinky": ["Bone.010", "Bone.011"], "thumb": ["Bone.012", "Bone.013"]},
          "Right": {"index": ["Bone.010", "Bone.011", "Bone.012"], "middle": ["Bone.001", "Bone.002", "Bone.003"],
                    "ring": ["Bone.004", "Bone.005", "Bone.006"], "pinky": ["Bone.007", "Bone.008", "Bone.009"], "thumb": ["Bone.013", "Bone.014"]}}
sfx = "_l" if side == "Left" else "_r"
rename = {"Bone": "hand" + sfx}
for finger, bones in CHAINS[side].items():
    for i, b in enumerate(bones): rename[b] = "%s_%02d%s" % (finger, i + 1, sfx)

sc = bpy.context.scene
arm_n, mesh_n = PAIRS[side]
cam = sc.camera
cam_matrix = cam.matrix_world.copy()
cam_lens, cam_sensor = cam.data.lens, cam.data.sensor_width   # read before the camera is deleted below
for o in list(sc.objects):
    if o.name not in (arm_n, mesh_n): bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)
arm = bpy.data.objects[arm_n]; hi = bpy.data.objects[mesh_n]
log("start", side, "verts", len(hi.data.vertices), "tris", sum(len(p.vertices) - 2 for p in hi.data.polygons))

# 1. express the mesh in the armature's own space and reset the armature object (its placement is kept in the json)
def sel(objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
arm_matrix = arm.matrix_world.copy()
sel([hi]); bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
hi.matrix_world = arm_matrix.inverted() @ hi.matrix_world
arm.matrix_world = Matrix.Identity(4)
sel([hi]); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
hi.parent = arm; hi.matrix_parent_inverse = Matrix.Identity(4)
for m in hi.modifiers:
    if m.type == 'ARMATURE': m.object = arm
for old, new in rename.items():
    arm.data.bones[old].name = new
for g in hi.vertex_groups:
    if g.name in rename: g.name = rename[g.name]
missing = [b.name for b in arm.data.bones if b.name not in hi.vertex_groups]
log("bones", [b.name for b in arm.data.bones], "bones without weights:", missing)
pts = [hi.matrix_world @ v.co for v in hi.data.vertices]
lo_b = [min(p[i] for p in pts) for i in range(3)]; hi_b = [max(p[i] for p in pts) for i in range(3)]
log("armature-space bbox:", [round(x, 3) for x in lo_b], [round(x, 3) for x in hi_b])
root = arm.data.bones["hand" + sfx]
tips = [b for b in arm.data.bones if not b.children]
hand_len = max((t.tail_local - root.head_local).length for t in tips)
hand_len_world = max((arm_matrix @ t.tail_local - arm_matrix @ root.head_local).length for t in tips)
log("hand length: armature units", round(hand_len, 3), "| in the scene (m)", round(hand_len_world, 3))
with open(os.path.join(out_dir, "FPHand_%s_placement.json" % side[0]), "w") as f:
    json.dump({"side": side, "scene_file": bpy.data.filepath, "armature_matrix": [list(r) for r in arm_matrix],
               "camera_matrix": [list(r) for r in cam_matrix], "camera_lens_mm": cam_lens,
               "camera_sensor_mm": cam_sensor, "hand_length_armature": hand_len,
               "hand_length_scene_m": hand_len_world}, f, indent=1)

# 2. low-poly copy: strip UVs, collapse-decimate, smart UV
hi.name = "FPHand%s_high" % sfx
lo = hi.copy(); lo.data = hi.data.copy(); lo.name = "FPHand" + sfx
sc.collection.objects.link(lo)
while lo.data.uv_layers: lo.data.uv_layers.remove(lo.data.uv_layers[0])
lo.data.materials.clear()
bm = bmesh.new(); bm.from_mesh(lo.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=hand_len * 5e-6)
bm.to_mesh(lo.data); bm.free()
sel([lo])
cur = len(lo.data.polygons)
while cur > target * 1.02:
    m = lo.modifiers.new("dec", 'DECIMATE'); m.decimate_type = 'COLLAPSE'
    m.ratio = max(target / cur, 0.3); m.use_collapse_triangulate = True
    bpy.ops.object.modifier_move_to_index(modifier=m.name, index=0)
    bpy.ops.object.modifier_apply(modifier=m.name)
    new = len(lo.data.polygons); log("decimate", cur, "->", new)
    if new >= cur * 0.98: break
    cur = new
bpy.ops.object.vertex_group_clean(group_select_mode='ALL', limit=0.01)
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
log("weights: max influences", max(len(v.groups) for v in lo.data.vertices), "unweighted verts", sum(1 for v in lo.data.vertices if not v.groups))
lo.data.shade_smooth()
lo.data.uv_layers.new(name="UVMap")
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.002, area_weight=0.0, scale_to_bounds=True)
try: bpy.ops.uv.pack_islands(rotate=True, margin=0.002)
except Exception as e: log("pack skipped", e)
bpy.ops.object.mode_set(mode='OBJECT')
log("low", len(lo.data.polygons), "tris")

# 3. bake the four maps from the high-poly mesh (distances relative to the hand: armature units)
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1
b = sc.render.bake
b.use_selected_to_active = True; b.cage_extrusion = hand_len * 0.0105; b.max_ray_distance = hand_len * 0.0316
b.margin = 16; b.margin_type = 'EXTEND'
name = "FPHand_" + side[0]
hmat = hi.data.materials[0]; hnt = hmat.node_tree
hb = next(n for n in hnt.nodes if n.type == 'BSDF_PRINCIPLED')
hout = next(n for n in hnt.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
emit = hnt.nodes.new('ShaderNodeEmission')
lmat = bpy.data.materials.new("M_" + name); lmat.use_nodes = True; lo.data.materials.append(lmat)
lnt = lmat.node_tree; lb = next(n for n in lnt.nodes if n.type == 'BSDF_PRINCIPLED')
sel([hi, lo]); bpy.context.view_layer.objects.active = lo
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

# 4. export: armature + low mesh, no animation (the placed pose is the reference pose)
bpy.data.objects.remove(hi, do_unlink=True)
lo.parent = arm; lo.matrix_parent_inverse = Matrix.Identity(4)
sel([arm, lo])
bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, "SK_%s.fbx" % name), use_selection=True,
    object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=False, mesh_smooth_type='FACE', use_tspace=True,
    path_mode='STRIP', primary_bone_axis='Y', secondary_bone_axis='X')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "%s_low.blend" % name))
log("DONE", name, len(lo.data.polygons), "tris")
