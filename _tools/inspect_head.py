import unreal
S = unreal.AnimSequenceService
SK = "/Game/Character/SK_Hunyuan"
path = "/Game/Character/A_Idle"

sk = unreal.load_asset(SK)
B = sk.get_bounds()
scale = (92.0 * 2.0) / (B.box_extent.z * 2.0)
mesh_world_z = -((B.origin.z - B.box_extent.z) * scale)
unreal.log("runtime scale=%.4f meshWorldZ=%.2f" % (scale, mesh_world_z))

poses = S.get_pose_at_time(path, 0.0, True)  # component space
for p in poses:
    nm = str(p.bone_name)
    low = nm.lower()
    if any(k in low for k in ("head", "eye", "neck", "face")):
        t = p.transform.translation
        world = mesh_world_z + t.z * scale
        cap = world - 92.0
        unreal.log("%-18s comp=(%.1f,%.1f,%.1f) world.z=%.2f capRelZ=%.2f" %
                   (nm, t.x, t.y, t.z, world, cap))
unreal.log("INSPECT HEAD DONE")
