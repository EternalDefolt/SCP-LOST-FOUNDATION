import unreal

# Replicate BodycamCharacter.cpp runtime mesh placement to find where the planted
# foot REALLY lands in world, so we can ground the anims to the value the C++ expects.
#   ScaleFactor  = (2*HalfHeight) / (2*BoxExtent.Z)
#   ScaledBottom = (Bounds.Origin.Z - BoxExtent.Z) * ScaleFactor
#   MeshWorldZ   = CapsuleCenter(HalfHeight) + (-HalfHeight - ScaledBottom) = -ScaledBottom
#   worldFoot    = MeshWorldZ + lowestFootComp * ScaleFactor
SK = "/Game/Character/SK_Hunyuan"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
HALF = 92.0
S = unreal.AnimSequenceService

sk = unreal.load_asset(SK)
B = sk.get_bounds()
ext = B.box_extent
org = B.origin
mesh_h = ext.z * 2.0
scale = (HALF * 2.0) / mesh_h
bounds_bottom = org.z - ext.z
scaled_bottom = bounds_bottom * scale
mesh_world_z = -scaled_bottom
unreal.log("BOUNDS origin.z=%.2f extent.z=%.2f -> meshH=%.2f scale=%.4f boundsBottom=%.2f" %
           (org.z, ext.z, mesh_h, scale, bounds_bottom))
unreal.log("MeshWorldZ=%.2f (foot at comp 0 lands here in world)" % mesh_world_z)

# the anim's lowest foot must satisfy: mesh_world_z + foot*scale = 0  -> foot = scaled_bottom/scale = bounds_bottom
unreal.log("=> TARGET lowest foot (component) so world=0 is %.3f" % bounds_bottom)

for name in ("A_Idle", "A_Walk"):
    path = "/Game/Character/" + name
    a = unreal.load_asset(path)
    if a is None:
        unreal.log("%s MISSING" % name); continue
    n = a.get_editor_property("controller").get_model_interface().get_number_of_keys()
    dur = a.get_editor_property("sequence_length")
    lo = 1e9
    for f in range(n):
        tt = dur * f / max(n - 1, 1)
        lo = min(lo, min(p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)))
    world = mesh_world_z + lo * scale
    unreal.log("%s lowestFootComp=%.2f -> worldFoot=%.2f (0 = on floor, + = floating)" % (name, lo, world))

unreal.log("MEASURE DONE")
