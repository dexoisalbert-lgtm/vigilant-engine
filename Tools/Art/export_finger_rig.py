# usage: blender -b <Diver_fingers.blend> --python export_all.py -- <out_dir>
# Writes SK_Diver.fbx (body, finger bones, walk-frame-1 reference pose like the first import), SK_Diver_FPArm.fbx
# (left arm only, same skeleton) and A_Diver_FP_LeftArm.fbx (one-frame pose: straight arm, relaxed fingers).
import bpy, sys, os, bmesh, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pose_lib
out = sys.argv[sys.argv.index("--") + 1]
os.makedirs(out, exist_ok=True)
sc = bpy.context.scene
arm = next(o for o in sc.objects if o.type == 'ARMATURE'); mesh = next(o for o in sc.objects if o.type == 'MESH')
act = arm.animation_data.action if arm.animation_data else None
print("EXPORT action", act.name if act else None, "frames", sc.frame_start, sc.frame_end, "current", sc.frame_current, "mesh", mesh.name, "bones", len(arm.data.bones))
sc.frame_set(sc.frame_start)
arm.data.pose_position = 'POSE'
FBX = dict(add_leaf_bones=False, bake_anim_simplify_factor=0.0, mesh_smooth_type='FACE', use_tspace=True,
           path_mode='STRIP', primary_bone_axis='Y', secondary_bone_axis='X')

def sel(objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]

# 1. body
sel([arm, mesh])
bpy.ops.export_scene.fbx(filepath=os.path.join(out, "SK_Diver.fbx"), use_selection=True, object_types={'ARMATURE', 'MESH'},
    bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, **FBX)
print("EXPORT body done")

# 2. left arm only
chain = {g.name for g in mesh.vertex_groups if g.name in ("LeftArm", "LeftForeArm", "LeftHand") or g.name.startswith("LeftHand")}
gname = {g.index: g.name for g in mesh.vertex_groups}
keep = set()
for v in mesh.data.vertices:
    if sum(g.weight for g in v.groups if gname[g.group] in chain) > 0.5: keep.add(v.index)
fp = mesh.copy(); fp.data = mesh.data.copy(); fp.name = "Diver_FPArm"
sc.collection.objects.link(fp)
bm = bmesh.new(); bm.from_mesh(fp.data)
bad = [f for f in bm.faces if not all(v.index in keep for v in f.verts)]
bmesh.ops.delete(bm, geom=bad, context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(fp.data); bm.free()
print("EXPORT FP arm", len(fp.data.vertices), "verts", len(fp.data.polygons), "tris")
mesh.hide_set(True)
sel([arm, fp])
bpy.ops.export_scene.fbx(filepath=os.path.join(out, "SK_Diver_FPArm.fbx"), use_selection=True, object_types={'ARMATURE', 'MESH'},
    bake_anim=False, **FBX)
mesh.hide_set(False)
bpy.data.objects.remove(fp, do_unlink=True)

# 3. pose animation (armature only)
arm.animation_data.action = None
for pb in arm.pose.bones: pb.rotation_mode = 'XYZ'; pb.rotation_euler = (0, 0, 0); pb.location = (0, 0, 0); pb.scale = (1, 1, 1)
pose_lib.apply_fingers(arm, "Left", pose_lib.RELAXED_OPEN)
sc.frame_start, sc.frame_end = 1, 2
for fr in (1, 2):
    for pb in arm.pose.bones:
        pb.keyframe_insert("rotation_euler", frame=fr); pb.keyframe_insert("location", frame=fr)
arm.animation_data.action.name = "FP_LeftArm"
sel([arm])
bpy.ops.export_scene.fbx(filepath=os.path.join(out, "A_Diver_FP_LeftArm.fbx"), use_selection=True, object_types={'ARMATURE'},
    bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, **FBX)
print("EXPORT anim done")
