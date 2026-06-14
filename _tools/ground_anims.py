import unreal

# Ground BOTH anims so the planted foot lands on the WORLD floor given how
# BodycamCharacter.cpp places the mesh: it scales the mesh to the capsule and offsets
# by the static bounds bottom. Measured: a foot at component 0 floats to world +9.91.
# The component height that lands on world 0 is exactly the mesh bounds bottom.
#   target_foot_comp = Bounds.Origin.Z - Bounds.BoxExtent.Z
# Pure vertical Hips translation, idempotent (no x100 compounding).
SK = "/Game/Character/SK_Hunyuan"
S = unreal.AnimSequenceService
eal = unreal.EditorAssetLibrary

# the foot BONE sits a little above the skin sole; landing the bone on world 0 leaves
# the visible sole hovering. Drop both anims by this much in WORLD cm to seat the sole.
EXTRA_WORLD = 2.5

sk = unreal.load_asset(SK)
B = sk.get_bounds()
scale = (92.0 * 2.0) / (B.box_extent.z * 2.0)
target = (B.origin.z - B.box_extent.z) - EXTRA_WORLD / scale
unreal.log("MESH bounds bottom=%.3f scale=%.4f EXTRA_WORLD=%.1f -> target foot comp=%.3f" %
           (B.origin.z - B.box_extent.z, scale, EXTRA_WORLD, target))

for name in ("A_Idle", "A_Walk"):
    path = "/Game/Character/" + name
    a = unreal.load_asset(path)
    if a is None:
        unreal.log("%s : MISSING, skip" % name); continue
    ctrl = a.get_editor_property("controller")
    n = ctrl.get_model_interface().get_number_of_keys()
    dur = a.get_editor_property("sequence_length")

    lo = 1e9
    for f in range(n):
        tt = dur * f / max(n - 1, 1)
        lo = min(lo, min(p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)))
    shift = target - lo
    unreal.log("%s : lowestFoot=%.2f -> shift Hips %+.2f" % (name, lo, shift))

    pos, rots, sca = [], [], []
    for f in range(n):
        tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
        t = tr.translation
        pos.append(unreal.Vector(t.x, t.y, t.z + shift))
        rots.append(tr.rotation)
        sca.append(unreal.Vector(100.0, 100.0, 100.0))
    ctrl.open_bracket("ground", True)
    ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
    ctrl.close_bracket(True)

    # persist (save returns False if a stale editor holds the file lock)
    ok = eal.save_asset(path, False)
    unreal.log("%s : save_asset ok=%s" % (name, ok))

    lo2 = 1e9
    for f in range(n):
        tt = dur * f / max(n - 1, 1)
        lo2 = min(lo2, min(p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)))
    unreal.log("%s : DONE lowestFoot now=%.2f (target %.2f)" % (name, lo2, target))

unreal.log("GROUND ANIMS DONE")
