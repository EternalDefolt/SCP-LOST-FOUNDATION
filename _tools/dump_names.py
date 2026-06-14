import unreal
S = unreal.AnimSequenceService
poses = S.get_pose_at_time("/Game/Character/A_Run", 0.5, True)
names = [str(p.bone_name) for p in poses]
hand = [n for n in names if "Hand" in n]
unreal.log("HAND BONES: %s" % hand)
