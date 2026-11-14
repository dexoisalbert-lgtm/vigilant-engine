import math
# Relaxed grip: flexion in degrees per finger [proximal, middle, distal]; negative X rotation flexes toward the palm.
RELAXED = {"Index": (-24, -32, -20), "Middle": (-28, -36, -22), "Ring": (-32, -40, -24), "Pinky": (-36, -44, -26), "Thumb": (0, -14, -14)}
FIST = {"Index": (-80, -90, -60), "Middle": (-80, -90, -60), "Ring": (-80, -90, -60), "Pinky": (-80, -90, -60), "Thumb": (0, -40, -50)}
def apply_fingers(arm, side, preset):
    for f, angs in preset.items():
        for i, a in enumerate(angs):
            pb = arm.pose.bones["%sHand%s%d" % (side, f, i + 1)]
            pb.rotation_mode = 'XYZ'; pb.rotation_euler = (math.radians(a), 0, 0)
# Watch-check pose for the first-person arm: fingers only slightly curled.
RELAXED_OPEN = {"Index": (-14, -20, -12), "Middle": (-18, -24, -14), "Ring": (-22, -28, -16), "Pinky": (-26, -32, -18), "Thumb": (0, -8, -8)}
