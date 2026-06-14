import unreal
S = unreal.AnimSequenceService
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"

ref_floor = min(b.transform.translation.z for b in S.get_reference_pose(SKEL))
unreal.log("REF pose lowest foot = %.2f" % ref_floor)

for name in ("A_Idle", "A_Walk"):
    path = "/Game/Character/" + name
    a = unreal.load_asset(path)
    if a is None:
        unreal.log("%s : MISSING" % name); continue
    ctrl = a.get_editor_property("controller")
    n = ctrl.get_model_interface().get_number_of_keys()
    dur = a.get_editor_property("sequence_length")
    lo = 1e9; hi = -1e9; hips0 = None
    for f in range(n):
        tt = dur * f / max(n - 1, 1)
        zs = [p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)]
        lo = min(lo, min(zs)); hi = max(hi, max(zs))
    hipsz = [S.get_bone_transform_at_frame(path, "Hips", f, False).translation.z for f in range(n)]
    unreal.log("%s : keys=%d lowestFoot=%.2f topHead=%.2f height=%.1f HipsZ min=%.2f max=%.2f" %
               (name, n, lo, hi, hi - lo, min(hipsz), max(hipsz)))
unreal.log("INSPECT FEET DONE")
