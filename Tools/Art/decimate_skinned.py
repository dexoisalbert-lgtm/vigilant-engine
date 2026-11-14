# usage: blender -b --python player_lod.py -- <src.fbx> <out_dir> <name> <target_tris> <bake_res>
import bpy, bmesh, sys, os, time, math
a = sys.argv[sys.argv.index("--")+1:]
src, out_dir, name, target, res = a[0], a[1], a[2], int(a[3]), int(a[4])
os.makedirs(out_dir, exist_ok=True)
T = time.time()
def log(*x): print("[pl]", *x, "t=%.0fs" % (time.time()-T), flush=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
sc = bpy.context.scene
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
hi = next(o for o in sc.objects if o.type == 'MESH')
arm.data.pose_position = 'REST'
hi.name = name + "_high"
lo = hi.copy(); lo.data = hi.data.copy(); lo.name = name
sc.collection.objects.link(lo)
while lo.data.uv_layers: lo.data.uv_layers.remove(lo.data.uv_layers[0])
lo.data.materials.clear()
bm = bmesh.new(); bm.from_mesh(lo.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
bm.to_mesh(lo.data); bm.free()
for o in sc.objects: o.select_set(o == lo)
bpy.context.view_layer.objects.active = lo
cur = len(lo.data.polygons)
while cur > target * 1.02:
    m = lo.modifiers.new("dec", 'DECIMATE'); m.decimate_type = 'COLLAPSE'
    m.ratio = max(target / cur, 0.3); m.use_collapse_triangulate = True
    bpy.ops.object.modifier_move_to_index(modifier=m.name, index=0)  # before the armature: rest shape only
    bpy.ops.object.modifier_apply(modifier=m.name)
    new = len(lo.data.polygons); log("decimate", cur, "->", new)
    if new >= cur * 0.98: break
    cur = new
bpy.ops.object.vertex_group_clean(group_select_mode='ALL', limit=0.01)
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
unweighted = sum(1 for v in lo.data.vertices if not v.groups)
log("weights: max influences", max(len(v.groups) for v in lo.data.vertices), "unweighted verts", unweighted)
lo.data.shade_smooth()
lo.data.uv_layers.new(name="UVMap")
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.002, area_weight=0.0, scale_to_bounds=True)
try: bpy.ops.uv.pack_islands(rotate=True, margin=0.002)
except Exception as e: log("pack skipped", e)
bpy.ops.object.mode_set(mode='OBJECT')
log("low", len(lo.data.polygons), "tris")

sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1
b = sc.render.bake
b.use_selected_to_active = True; b.cage_extrusion = 0.6; b.max_ray_distance = 1.5  # armature space is cm
b.margin = 16; b.margin_type = 'EXTEND'
hmat = hi.data.materials[0]; hnt = hmat.node_tree
hb = next(n for n in hnt.nodes if n.type == 'BSDF_PRINCIPLED')
hout = next(n for n in hnt.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
emit = hnt.nodes.new('ShaderNodeEmission')
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
arm.data.pose_position = 'POSE'
for o in sc.objects: o.select_set(o in (arm, lo))
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, "SK_%s.fbx" % name), use_selection=True,
    object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
    bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0.0, mesh_smooth_type='FACE', use_tspace=True,
    path_mode='STRIP', primary_bone_axis='Y', secondary_bone_axis='X')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "%s_low.blend" % name))
log("DONE ->", len(lo.data.polygons))
