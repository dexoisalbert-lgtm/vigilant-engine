# usage: blender -b <Diver_fingers.blend> --python check_finger_pose.py -- <out_dir> <Left|Right> <RELAXED|FIST|RELAXED_OPEN>
# Renders the hand in a finger pose (top, front, bottom) to check the weights.
import bpy, sys, os, math
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import pose_lib
from mathutils import Vector
a = sys.argv[sys.argv.index("--")+1:]
outdir, side, presetname = a[0], a[1], a[2]
os.makedirs(outdir, exist_ok=True)
sc = bpy.context.scene
arm = next(o for o in sc.objects if o.type == 'ARMATURE'); mesh = next(o for o in sc.objects if o.type == 'MESH')
if arm.animation_data: arm.animation_data.action = None
arm.data.pose_position = 'POSE'
for pb in arm.pose.bones: pb.rotation_mode = 'XYZ'; pb.rotation_euler = (0, 0, 0); pb.location = (0, 0, 0)
pose_lib.apply_fingers(arm, side, getattr(pose_lib, presetname))
bpy.context.view_layer.update()
mesh.color = (0.78, 0.74, 0.7, 1)
sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading; sh.light = 'STUDIO'; sh.color_type = 'OBJECT'
sc.render.resolution_x = 1000; sc.render.resolution_y = 800
w = bpy.data.worlds.new("w"); w.color = (0.2, 0.2, 0.22); sc.world = w
sx = 1 if side == "Left" else -1
cx, cy, cz = 0.76 * sx, 0.03, 1.32
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam)
cam.data.type = 'ORTHO'; cam.data.ortho_scale = 0.30; sc.camera = cam
views = {"top": ((cx, cy, cz + 2), (0, 0, 0)), "front": ((cx, cy - 2, cz), (math.radians(90), 0, 0)),
         "bottom": ((cx, cy, cz - 2), (math.radians(180), 0, 0))}
for name, (loc, rot) in views.items():
    cam.location = loc; cam.rotation_euler = rot
    sc.render.filepath = os.path.join(outdir, "pose_%s_%s_%s.png" % (presetname, side, name))
    bpy.ops.render.render(write_still=True)
print("POSE DONE")
