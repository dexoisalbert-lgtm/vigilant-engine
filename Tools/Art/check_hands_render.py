# usage: blender -b <FPHand_L_low.blend> --python check_hands_render.py -- <out.png> [<FPHand_R_low.blend>]
# Renders the exported hands through a camera set up like the game camera (50 mm lens, 36 mm sensor, at the origin looking +X).
import bpy, sys, math, os
a = sys.argv[sys.argv.index("--")+1:]
out = a[0]
for extra in a[1:]:
    with bpy.data.libraries.load(extra) as (src, dst):
        dst.objects = [n for n in src.objects]
    for o in dst.objects:
        if o is not None: bpy.context.scene.collection.objects.link(o)
sc = bpy.context.scene
for m in bpy.data.materials:
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image and n.image.name.endswith('_BC'): m.node_tree.nodes.active = n
cam = bpy.data.objects.new("qa_cam", bpy.data.cameras.new("qa_cam")); sc.collection.objects.link(cam)
cam.data.lens = 50; cam.data.sensor_width = 36; cam.data.sensor_fit = 'AUTO'
cam.location = (0, 0, 0); cam.rotation_euler = (math.radians(90), 0, math.radians(-90)); sc.camera = cam
sc.render.resolution_x, sc.render.resolution_y = 1280, 720
sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading; sh.light = 'STUDIO'; sh.color_type = 'TEXTURE'
w = bpy.data.worlds.new("w"); w.color = (0.24, 0.24, 0.25); sc.world = w
sc.render.filepath = out
bpy.ops.render.render(write_still=True)
print("QA DONE")
