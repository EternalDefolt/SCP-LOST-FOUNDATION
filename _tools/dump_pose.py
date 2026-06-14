import unreal
S = unreal.AnimSequenceService
poses = S.get_pose_at_time("/Game/Character/A_Run", 0.5, True)
p0 = poses[0]
unreal.log("POSE ELEM TYPE: %s" % type(p0))
unreal.log("ATTRS: %s" % [a for a in dir(p0) if not a.startswith('_')])
names = []
for p in poses[:8]:
    try: names.append(str(p.bone_name))
    except Exception as e: names.append("ERR:%s"%e)
unreal.log("FIRST NAMES: %s" % names)
unreal.log("count=%d" % len(poses))
