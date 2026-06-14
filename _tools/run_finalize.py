import unreal
path = "/Game/Character/A_Run"
SK = "/Game/Character/SK_Hunyuan"
S = unreal.AnimSequenceService
eal = unreal.EditorAssetLibrary
anim = unreal.load_asset(path)
ctrl = anim.get_editor_property("controller")
n = ctrl.get_model_interface().get_number_of_keys()
dur = anim.get_editor_property("sequence_length")
unreal.log("A_Run keys=%d length=%.3f" % (n, dur))

# sanity: right-hand vertical travel over the cycle => proves run arm swing present
def boneZrange(bone):
    zs = []
    for f in range(n):
        tt = dur * f / max(n-1,1)
        for p in S.get_pose_at_time(path, tt, True):
            if p.bone_name == bone:
                zs.append(p.transform.translation.z)
    return (max(zs)-min(zs)) if zs else -1.0
unreal.log("RightHand Zrange=%.2f  LeftHand Zrange=%.2f" % (boneZrange("RightHand"), boneZrange("LeftHand")))

# rootfix: keep imported Hips pos/rot, force scale 100
pos, rots = [], []
for f in range(n):
    tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
    pos.append(tr.translation); rots.append(tr.rotation)
sca = [unreal.Vector(100.0,100.0,100.0) for _ in range(n)]
ctrl.open_bracket("rootfix", True)
ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
ctrl.close_bracket(True)
eal.save_asset(path, False)

# ground: shift Hips Z so lowest foot over cycle == mesh bounds bottom
sk = unreal.load_asset(SK)
B = sk.get_bounds()
target = B.origin.z - B.box_extent.z
def lowest():
    lo = 1e9
    for f in range(n):
        tt = dur * f / max(n-1,1)
        lo = min(lo, min(p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)))
    return lo
lo = lowest(); shift = target - lo
pos2, rots2, sca2 = [], [], []
for f in range(n):
    tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
    tt = tr.translation
    pos2.append(unreal.Vector(tt.x, tt.y, tt.z + shift))
    rots2.append(tr.rotation); sca2.append(unreal.Vector(100.0,100.0,100.0))
ctrl.open_bracket("ground", True)
ctrl.set_bone_track_keys("Hips", pos2, rots2, sca2, True)
ctrl.close_bracket(True)
ok = eal.save_asset(path, False)
unreal.log("A_Run grounded target=%.2f wasLowest=%.2f shift=%.2f saveOK=%s nowLowest=%.2f" %
           (target, lo, shift, ok, lowest()))
unreal.log("RUN FINALIZE DONE")
