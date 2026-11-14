# usage: blender -b <low.blend> --python gear_compare.py -- <src.fbx> <out.png> [wire]
import bpy, sys, mathutils, math
a = sys.argv[sys.argv.index("--")+1:]
src, outp = a[0], a[1]
wire = len(a) > 2
sc = bpy.context.scene
lo = next(o for o in sc.objects if o.type == 'MESH')
real = max(lo.dimensions)
bpy.ops.import_scene.fbx(filepath=src)
hi = next(o for o in bpy.context.selected_objects if o.type == 'MESH')
s = real / max(hi.dimensions); hi.scale = [c*s for c in hi.scale]
bpy.context.view_layer.update()
bb = [hi.matrix_world @ mathutils.Vector(c) for c in hi.bound_box]
c = sum(bb, mathutils.Vector())/8
off = real * 0.6
hi.location = hi.location - c + mathutils.Vector((-off, 0, 0))
lo.location = (off, 0, 0)
if wire:
    w = lo.modifiers.new("w", 'WIREFRAME'); w.thickness = real*0.002; w.use_replace = False
    w.material_offset = 1
    m = bpy.data.materials.new("wire"); m.use_nodes = True
    next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Base Color'].default_value = (0,0,0,1)
    lo.data.materials.append(m)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam)
d = mathutils.Vector((0.0, -1.0, 0.35)).normalized()
cam.data.type = 'ORTHO'; cam.data.ortho_scale = real*2.5
cam.location = d * real * 4
cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
sc.camera = cam
for ang, e in ((math.radians(50), 3.0), (math.radians(-120), 1.5)):
    L = bpy.data.objects.new("l", bpy.data.lights.new("l", 'SUN')); L.data.energy = e
    L.rotation_euler = (math.radians(55), 0, ang); sc.collection.objects.link(L)
sc.world = sc.world or bpy.data.worlds.new("w"); sc.world.use_nodes = True
sc.world.node_tree.nodes['Background'].inputs[0].default_value = (0.25,0.27,0.3,1)
try: sc.render.engine = 'BLENDER_EEVEE_NEXT'
except TypeError: sc.render.engine = 'BLENDER_EEVEE'
sc.render.resolution_x, sc.render.resolution_y = 1400, 700
sc.render.filepath = outp
bpy.ops.render.render(write_still=True)
print("[cmp] wrote", outp)
