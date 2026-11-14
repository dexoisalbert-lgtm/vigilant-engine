# usage: blender -b <Diver_low.blend> --python rig_fingers.py -- <out.blend>
# Adds 15 finger bones per hand (thumb, index, middle, ring, pinky; 3 phalanges each) to the decimated diver and
# re-weights the hand vertices by distance along each finger. Armature units are cm (object scale 0.01), the mesh
# is in metres; all analysis here is in world metres. The rest pose is a T-pose with the palms facing down.
import bpy, sys, math
from mathutils import Vector

out_blend = sys.argv[sys.argv.index("--") + 1]
sc = bpy.context.scene
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
mesh = next(o for o in sc.objects if o.type == 'MESH')
arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
mw = mesh.matrix_world
aw = arm.matrix_world
awi = aw.inverted()
vg_by_name = {g.name: g for g in mesh.vertex_groups}

FINGERS = ["Index", "Middle", "Ring", "Pinky"]
# Initial centre-line estimates read off the T-pose hand (x mirrored so a hand is always at +x): (y, z) at x=0.77, tip (x, y)
INIT = {"Index": ((0.018, 0.0365), (0.839, 0.025)), "Middle": ((0.039, 0.0365), (0.851, 0.044)),
        "Ring": ((0.062, 0.0365), (0.843, 0.066)), "Pinky": ((0.086, 0.0365), (0.823, 0.084))}
YC = {"Index": 0.0265, "Middle": 0.0436, "Ring": 0.0625, "Pinky": 0.085}
WRIST_X = 0.639
PHALANX = (0.44, 0.30, 0.26)     # fraction of the MCP->tip length
TUBE = 0.0105


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def fit_finger(pts, name):
    # The fingers run almost parallel to x, so the lateral position is fixed (read off the hand) and only the height is fitted.
    yc = YC[name]
    tx = INIT[name][1][0]
    c, d = 1.4115, -0.0947     # shared dorsal slope: z = 1.341 at the knuckles, 1.332 at the tips
    sel = [p for p in pts if 0.77 <= p.x <= tx + 0.012 and abs(p.y - yc) < 0.0095 and abs(p.z - (c + d * p.x)) < 0.016]
    tip_x = max(p.x for p in sel) - 0.004
    mcp_x = tip_x - 0.47 * (tip_x - WRIST_X)
    P = lambda x: Vector((x, yc, c + d * x))
    j0 = P(mcp_x); j3 = P(tip_x)
    j1 = j0 + (j3 - j0) * PHALANX[0]
    j2 = j0 + (j3 - j0) * (PHALANX[0] + PHALANX[1])
    return [j0, j1, j2, j3], len(sel)


# Thumb chain estimated from the thumb surface (CMC -> MCP -> IP -> tip), x mirrored.
THUMB = [Vector((0.672, 0.030, 1.318)), Vector((0.712, 0.002, 1.311)), Vector((0.742, -0.011, 1.305)), Vector((0.764, -0.016, 1.301))]

rig = {}   # side -> {chain name: [4 joints]} in world metres, x mirrored per side
hand_w = {}  # side -> {vertex index: weight}
for side, s in (("Left", 1), ("Right", -1)):
    g = vg_by_name[side + "Hand"]
    W = {}
    pts = []
    for v in mesh.data.vertices:
        w = next((x.weight for x in v.groups if x.group == g.index), 0.0)
        if w > 0.0:
            W[v.index] = w
            if w > 0.3:
                p = mw @ v.co; pts.append(Vector((s * p.x, p.y, p.z)))
    hand_w[side] = W
    chains = {}
    for f in FINGERS:
        j, n = fit_finger(pts, f)
        chains[f] = j
        print("RIG", side, f, "tube verts", n, "MCP", tuple(round(c, 4) for c in j[0]), "tip", tuple(round(c, 4) for c in j[3]))
    chains["Thumb"] = [Vector(p) for p in THUMB]
    rig[side] = chains

# ---- bones
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
eb = arm.data.edit_bones
new_bones = {}
for side, s in (("Left", 1), ("Right", -1)):
    parent = eb[side + "Hand"]
    for f in ["Thumb"] + FINGERS:
        j = rig[side][f]
        prev = None
        for i in range(3):
            name = "%sHand%s%d" % (side, f, i + 1)
            b = eb.new(name)
            hw = lambda q: awi @ Vector((s * q.x, q.y, q.z))
            b.head = hw(j[i]); b.tail = hw(j[i + 1])
            b.align_roll(Vector((0, 0, 1)))
            b.use_deform = True
            if prev is None: b.parent = parent; b.use_connect = False
            else: b.parent = prev; b.use_connect = True
            prev = b
            new_bones[name] = (side, f, i)
bpy.ops.object.mode_set(mode='OBJECT')
print("BONES now", len(arm.data.bones))

# ---- weights
for name in new_bones:
    mesh.vertex_groups.new(name=name)
vgs = {g.name: g for g in mesh.vertex_groups}


def polyline_param(p, J, extend_back):
    best = None; cum = 0.0
    for k in range(3):
        a, b = J[k], J[k + 1]; ab = b - a; l2 = ab.length_squared; ln = math.sqrt(l2)
        t = (p - a).dot(ab) / l2
        lo = -10.0 if (k == 0 and extend_back) else 0.0
        t = max(lo, min(1.0, t))
        q = a + ab * t
        d = (p - q).length
        s = cum + t * ln
        if best is None or d < best[0]: best = (d, s)
        cum += ln
    return best


SIGMA = 0.007
stats = {}
for side, s in (("Left", 1), ("Right", -1)):
    chains = rig[side]
    lens = {f: [(chains[f][i + 1] - chains[f][i]).length for i in range(3)] for f in chains}
    for vi, wh in hand_w[side].items():
        p0 = mw @ mesh.data.vertices[vi].co
        p = Vector((s * p0.x, p0.y, p0.z))
        a = {}; sv = {}
        for f in chains:
            d, sp = polyline_param(p, chains[f], True)
            a[f] = math.exp(-(d / SIGMA) ** 2); sv[f] = sp
        tot = sum(a.values())
        share = {f: (a[f] / tot if tot > 1e-12 else 0.0) for f in chains}
        res = {}
        used = 0.0
        for f in chains:
            if share[f] <= 1e-4: continue
            if f == "Thumb": ramp = smoothstep((sv[f] + 0.005) / 0.025)
            else: ramp = smoothstep((sv[f] + 0.010) / 0.020)
            wf = share[f] * ramp
            L = lens[f]; d1 = 0.35 * min(L[0], L[1]); d2 = 0.35 * min(L[1], L[2])
            S1 = smoothstep((sv[f] - (L[0] - d1)) / (2 * d1))
            S2 = smoothstep((sv[f] - (L[0] + L[1] - d2)) / (2 * d2))
            parts = (1 - S1, S1 - S2, S2)
            for i in range(3):
                nm = "%sHand%s%d" % (side, f, i + 1)
                res[nm] = res.get(nm, 0.0) + wf * parts[i]
            used += wf
        for nm, w in res.items():
            if wh * w > 1e-4: vgs[nm].add([vi], wh * w, 'REPLACE')
        palm = wh * max(0.0, 1.0 - used)
        vgs[side + "Hand"].add([vi], palm, 'REPLACE')
        for nm in res: stats[nm] = stats.get(nm, 0) + 1

bpy.ops.object.select_all(action='DESELECT')
mesh.select_set(True); bpy.context.view_layer.objects.active = mesh
bpy.ops.object.vertex_group_clean(group_select_mode='ALL', limit=0.01)
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL', limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL', lock_active=False)
mx = max(len(v.groups) for v in mesh.data.vertices)
print("WEIGHTS max influences", mx, "verts per new bone (touched):", {k: v for k, v in sorted(stats.items())})
bpy.ops.wm.save_as_mainfile(filepath=out_blend)
print("SAVED", out_blend)
