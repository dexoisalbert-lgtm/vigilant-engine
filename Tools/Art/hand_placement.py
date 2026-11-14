"""Turns the hand placements written by hand_pipeline.py into camera-relative transforms for Unreal.

usage: py -3 Tools/Art/hand_placement.py <FPHand_L_placement.json> <FPHand_R_placement.json> [hand_length_cm]

In Blender each hand rig sits in front of the scene camera (the player's eyes). The exported hand asset is in the
rig's own armature space, so in Unreal the hand component needs the transform "armature space -> camera space":

    camera point (cm) = scale * Q * asset point (cm) + T

Both hands share one uniform factor that turns the scene into real size (a hand of hand_length_cm, 19 by default).
Scaling everything about the camera does not change the picture, only how far away things really are.
Prints the C++ lines for PaleotrenchDiver.cpp (MakeHandPlacement) and a check of the numbers.
"""
import json, math, sys


def matmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(len(b))) for j in range(len(b[0]))] for i in range(len(a))]


def inverse4(m):
    # Gauss-Jordan; fine for well-conditioned 4x4 transforms.
    n = 4
    a = [list(map(float, row)) + [1.0 if i == j else 0.0 for j in range(n)] for i, row in enumerate(m)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(a[r][col]))
        a[col], a[piv] = a[piv], a[col]
        p = a[col][col]
        a[col] = [x / p for x in a[col]]
        for r in range(n):
            if r != col and a[r][col] != 0.0:
                f = a[r][col]
                a[r] = [x - f * y for x, y in zip(a[r], a[col])]
    return [row[n:] for row in a]


def det3(m):
    return (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
            + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]))


# Blender camera space (x right, y up, z back) -> Unreal camera space (x forward, y right, z up).
R_C = [[0, 0, -1], [1, 0, 0], [0, 1, 0]]
# Blender asset space -> Unreal asset space as the FBX round trip does it (Unreal y = -Blender y).
C = [[1, 0, 0], [0, -1, 0], [0, 0, 1]]


def placement(data, s):
    k = inverse4(data["camera_matrix"])
    p = matmul(k, data["armature_matrix"])
    lin = [row[:3] for row in p[:3]]
    t = [row[3] for row in p[:3]]
    full = [[s * x for x in row] for row in matmul(matmul(R_C, lin), C)]
    scale = abs(det3(full)) ** (1.0 / 3.0)
    q = [[x / scale for x in row] for row in full]
    translation = [100.0 * s * v for v in [sum(R_C[i][j] * t[j] for j in range(3)) for i in range(3)]]
    ortho = max(abs(sum(q[r][i] * q[r][j] for r in range(3)) - (1.0 if i == j else 0.0)) for i in range(3) for j in range(3))
    return q, translation, scale, ortho, det3(q)


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    left, right = (json.load(open(path)) for path in argv[1:3])
    hand_cm = float(argv[3]) if len(argv) > 3 else 19.0
    s = (hand_cm / 100.0) / ((left["hand_length_scene_m"] + right["hand_length_scene_m"]) / 2.0)
    print("// scene -> real size factor %.4f (hands %.3f / %.3f m in the scene -> %.1f cm)"
          % (s, left["hand_length_scene_m"], right["hand_length_scene_m"], hand_cm))
    print("// Blender camera: %.1f mm lens on a %.1f mm sensor = %.2f degrees across"
          % (left["camera_lens_mm"], left["camera_sensor_mm"],
             math.degrees(2.0 * math.atan(left["camera_sensor_mm"] / (2.0 * left["camera_lens_mm"])))))
    for name, data in (("Left", left), ("Right", right)):
        q, t, scale, ortho, det = placement(data, s)
        x = [q[0][0], q[1][0], q[2][0]]
        z = [q[0][2], q[1][2], q[2][2]]
        print("// %s: orthonormality error %.2e, det %.4f" % (name, ortho, det))
        print("%s: FTransform(FRotationMatrix::MakeFromXZ(FVector(%.6f, %.6f, %.6f), FVector(%.6f, %.6f, %.6f)).ToQuat(),"
              % (name, x[0], x[1], x[2], z[0], z[1], z[2]))
        print("        FVector(%.3f, %.3f, %.3f), FVector(%.6f))" % (t[0], t[1], t[2], scale))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
