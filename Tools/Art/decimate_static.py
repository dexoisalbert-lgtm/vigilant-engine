# usage: blender -b --python gear_lod.py -- <src.fbx> <out_dir> <name> <target_tris> <real_size_m> [bake_res]
import bpy, bmesh, sys, os, time, math
from mathutils import Vector, Matrix
a = sys.argv[sys.argv.index("--")+1:]
src, out_dir, name, target, real = a[0], a[1], a[2], int(a[3]), float(a[4])
res = int(a[5]) if len(a) > 5 else 2048
STEP = float(a[6]) if len(a) > 6 else 0.3
VOXEL = float(a[7]) if len(a) > 7 else 0.0  # fraction of the largest dimension; 0 = no remesh
os.makedirs(out_dir, exist_ok=True)
T = time.time()
def log(*x): print("[gear]", name, *x, "t=%.0fs" % (time.time()-T), flush=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
hi = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
hi.name = name + "_high"
s = real / max(hi.dimensions)
bpy.context.view_layer.objects.active = hi
for o in bpy.context.scene.objects: o.select_set(o == hi)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bb = [Vector(c) for c in hi.bound_box]
hi.data.transform(Matrix.Translation(-(sum(bb, Vector()) / 8)))
src_tris = len(hi.data.polygons)
log("imported", src_tris, "tris; scale factor", round(s, 4))

# Low copy without UVs, so decimation follows the shape only.
lo = hi.copy(); lo.data = hi.data.copy(); lo.name = name
bpy.context.scene.collection.objects.link(lo)
while lo.data.uv_layers: lo.data.uv_layers.remove(lo.data.uv_layers[0])
lo.data.materials.clear()
bm = bmesh.new(); bm.from_mesh(lo.data)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=max(hi.dimensions) * 1e-6)
bm.to_mesh(lo.data); bm.free()
for o in bpy.context.scene.objects: o.select_set(o == lo)
bpy.context.view_layer.objects.active = lo
if VOXEL > 0:
    rm = lo.modifiers.new("remesh", 'REMESH'); rm.mode = 'VOXEL'
    rm.voxel_size = max(hi.dimensions) * VOXEL; rm.adaptivity = 0.0
    bpy.ops.object.modifier_apply(modifier=rm.name)
    log("voxel remesh", len(lo.data.polygons), "faces")
    bm = bmesh.new(); bm.from_mesh(lo.data); bmesh.ops.triangulate(bm, faces=bm.faces); bm.to_mesh(lo.data); bm.free()
cur = len(lo.data.polygons)
while cur > target * 1.02:
    m = lo.modifiers.new("dec", 'DECIMATE'); m.decimate_type = 'COLLAPSE'
    m.ratio = max(target / cur, STEP); m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=m.name)
    new = len(lo.data.polygons); log("decimate", cur, "->", new)
    if new >= cur * 0.98: break
    cur = new
# Now go to real-world scale (both meshes) for the bake and export.
for ob in (hi, lo):
    ob.data.transform(Matrix.Scale(s, 4))
    ob.data.update()
# Tidy: drop loose bits and zero-area faces left by the collapse.
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
# Fresh UVs.
lo.data.uv_layers.new(name="UVMap")
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.004, area_weight=0.0, scale_to_bounds=True)
try: bpy.ops.uv.pack_islands(rotate=True, margin=0.004)
except Exception as e: log("pack skipped", e)
bpy.ops.object.mode_set(mode='OBJECT')
log("low", len(lo.data.polygons), "tris, UVs done")

# Bake setup.
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 1
sc.render.bake.use_selected_to_active = True
sc.render.bake.cage_extrusion = real * 0.015
sc.render.bake.max_ray_distance = real * 0.04
sc.render.bake.margin = 16; sc.render.bake.margin_type = 'EXTEND'
hmat = hi.data.materials[0]; hnt = hmat.node_tree
hb = next(n for n in hnt.nodes if n.type == 'BSDF_PRINCIPLED')
hout = next(n for n in hnt.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
emit = hnt.nodes.new('ShaderNodeEmission'); emit.inputs['Strength'].default_value = 1.0

lmat = bpy.data.materials.new("M_" + name); lmat.use_nodes = True
lo.data.materials.append(lmat)
lnt = lmat.node_tree; lb = next(n for n in lnt.nodes if n.type == 'BSDF_PRINCIPLED')
for o in sc.objects: o.select_set(o in (hi, lo))
bpy.context.view_layer.objects.active = lo

def bake(suffix, kind, src_socket=None, srgb=False):
    img = bpy.data.images.new("T_%s_%s" % (name, suffix), res, res, alpha=False)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    t = lnt.nodes.new('ShaderNodeTexImage'); t.image = img; lnt.nodes.active = t
    if kind == 'EMIT':
        if src_socket.links:
            hnt.links.new(src_socket.links[0].from_socket, emit.inputs['Color'])
        else:
            v = src_socket.default_value
            emit.inputs['Color'].default_value = tuple(v) if hasattr(v, '__len__') else (v, v, v, 1)
            for l in list(emit.inputs['Color'].links): hnt.links.remove(l)
        hnt.links.new(emit.outputs[0], hout.inputs['Surface'])
        bpy.ops.object.bake(type='EMIT')
        hnt.links.new(hb.outputs[0], hout.inputs['Surface'])
    else:
        bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT')
    img.filepath_raw = os.path.join(out_dir, img.name + ".png"); img.file_format = 'PNG'; img.save()
    log("baked", suffix)
    return t

tBC = bake("BC", 'EMIT', hb.inputs['Base Color'], srgb=True)
tM = bake("M", 'EMIT', hb.inputs['Metallic'])
tR = bake("R", 'EMIT', hb.inputs['Roughness'])
tN = bake("N", 'NORMAL')
lnt.links.new(tBC.outputs[0], lb.inputs['Base Color'])
lnt.links.new(tM.outputs[0], lb.inputs['Metallic'])
lnt.links.new(tR.outputs[0], lb.inputs['Roughness'])
nm = lnt.nodes.new('ShaderNodeNormalMap'); lnt.links.new(tN.outputs[0], nm.inputs['Color'])
lnt.links.new(nm.outputs[0], lb.inputs['Normal'])
for t in (tBC, tM, tR, tN): t.image.filepath = t.image.filepath_raw

bpy.data.objects.remove(hi, do_unlink=True)
for o in sc.objects: o.select_set(o == lo)
bpy.ops.export_scene.fbx(filepath=os.path.join(out_dir, "SM_%s.fbx" % name), use_selection=True,
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', use_tspace=True,
                         path_mode='STRIP', axis_forward='-Y', axis_up='Z')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, "%s_low.blend" % name))
log("DONE", src_tris, "->", len(lo.data.polygons))
