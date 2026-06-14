import unreal

# A_Walk is already on disk with the real IK motion, but rootfix v2 kept the Hips
# POSITION track in the metres-rig scale while the skeleton lives at x100. Result:
# standing (ref pose) is fine, but walking drops the pelvis to ~1cm -> legs clip
# through the floor, and the bob amplitude (~2cm in metres = 0.02 units) is invisible
# so the walk looks flat/linear.
#
# Fix: rebuild the Hips track ->
#   XY  = ref pose hips XY        (no skating, centred; capsule does forward travel)
#   Z   = ref hips Z + (animZ - mean animZ) * 100   (correct height + visible bob)
#   rot = keep imported            (weight-shift / pelvis sway preserved)
#   scale = 100                    (anti ant-size)
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
path = "/Game/Character/A_Walk"
S = unreal.AnimSequenceService
eal = unreal.EditorAssetLibrary

ref = {str(b.bone_name): b.transform for b in S.get_reference_pose(SKEL)}
hp = ref["Hips"].translation
unreal.log("REF Hips xyz = (%.3f, %.3f, %.3f)" % (hp.x, hp.y, hp.z))

anim = unreal.load_asset(path)
ctrl = anim.get_editor_property("controller")
dm = ctrl.get_model_interface()
n = dm.get_number_of_keys()

az, rots = [], []
for f in range(n):
    tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
    az.append(tr.translation.z)
    rots.append(tr.rotation)

zmean = sum(az) / n
unreal.log("ANIM Hips Z raw: min=%.4f max=%.4f mean=%.4f range=%.4f" %
           (min(az), max(az), zmean, max(az) - min(az)))

SCALE = 100.0
MARGIN = 0.5  # keep planted foot 0.5cm above floor so it never z-fights / clips


def write_hips(base_z):
    pos = [unreal.Vector(hp.x, hp.y, base_z + (az[f] - zmean) * SCALE) for f in range(n)]
    sca = [unreal.Vector(100.0, 100.0, 100.0) for _ in range(n)]
    ctrl.open_bracket("fix_walk_hips", True)
    ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
    ctrl.close_bracket(True)
    eal.save_asset(path, only_if_is_dirty=False)


# pass 1: place pelvis at ref height, then measure the lowest foot over the WHOLE cycle
write_hips(hp.z)
dur = anim.get_editor_property("sequence_length")
floor = 1e9
for f in range(n):
    t = dur * f / max(n - 1, 1)
    zs = [p.transform.translation.z for p in S.get_pose_at_time(path, t, True)]
    floor = min(floor, min(zs))
unreal.log("PASS1 lowest foot over cycle = %.2fcm (want 0)" % floor)

# pass 2: lift the whole pelvis so the lowest planted foot sits at +MARGIN
base_z = hp.z - floor + MARGIN
write_hips(base_z)

floor2 = 1e9
top = -1e9
for f in range(n):
    t = dur * f / max(n - 1, 1)
    zs = [p.transform.translation.z for p in S.get_pose_at_time(path, t, True)]
    floor2 = min(floor2, min(zs)); top = max(top, max(zs))
unreal.log("FIXED base=%.2f bob=+/-%.2fcm  lowest foot=%.2f (>=0 good)  height=%.1fcm" %
           (base_z, (max(az) - zmean) * SCALE, floor2, top - floor2))
unreal.log("FIX WALK HIPS DONE")
