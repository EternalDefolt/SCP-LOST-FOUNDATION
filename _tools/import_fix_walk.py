import unreal

# Self-contained + idempotent: re-import A_Walk from the FBX (raw metres Hips track)
# then apply the Hips fix ONCE. Never read-back-then-rescale (that compounds x100).
# No material work here (MaterialEditor crashes under -NullRHI; material already set).
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
path = "/Game/Character/A_Walk"
S = unreal.AnimSequenceService
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
skeleton = unreal.load_asset(SKEL)

# ---- 1) fresh anim import from FBX (raw metres) ----
ask = unreal.FbxAnimSequenceImportData()
ask.set_editor_property("convert_scene", True)
ask.set_editor_property("convert_scene_unit", True)
ask.set_editor_property("import_uniform_scale", 1.0)
ask.set_editor_property("remove_redundant_keys", False)
ask.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
ui = unreal.FbxImportUI()
ui.import_mesh = False
ui.import_as_skeletal = True
ui.import_animations = True
ui.import_materials = False
ui.import_textures = False
ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
ui.set_editor_property("skeleton", skeleton)
ui.set_editor_property("anim_sequence_import_data", ask)
t = unreal.AssetImportTask()
t.filename = ART + "/Hunyuan_Walk.fbx"
t.destination_path = "/Game/Character"
t.destination_name = "A_Walk"
t.automated = True
t.replace_existing = True
t.save = True
t.options = ui
tools.import_asset_tasks([t])
unreal.log("IMPORTED A_Walk -> %s" % list(t.imported_object_paths))
if eal.does_asset_exist("/Game/Character/A_Walk_PhysicsAsset"):
    eal.delete_asset("/Game/Character/A_Walk_PhysicsAsset")

# ---- 2) read RAW metres Hips track ----
ref = {str(b.bone_name): b.transform for b in S.get_reference_pose(SKEL)}
hp = ref["Hips"].translation
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
unreal.log("RAW Hips Z: min=%.4f max=%.4f mean=%.4f range=%.4f" % (min(az), max(az), zmean, max(az) - min(az)))

SCALE = 100.0
dur = anim.get_editor_property("sequence_length")

# the capsule/mesh offset is tuned to the STANDING (ref) pose, so the walk's planted
# foot must land at the SAME height as the ref pose's lowest foot - not at z=0.
# (z=0 made the character float above the floor while walking.)
ref_floor = min(b.transform.translation.z for b in S.get_reference_pose(SKEL))
unreal.log("REF pose lowest foot = %.2fcm (walk target)" % ref_floor)


def write_hips(base_z):
    pos = [unreal.Vector(hp.x, hp.y, base_z + (az[f] - zmean) * SCALE) for f in range(n)]
    sca = [unreal.Vector(100.0, 100.0, 100.0) for _ in range(n)]
    ctrl.open_bracket("fix_walk_hips", True)
    ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
    ctrl.close_bracket(True)
    eal.save_asset(path, only_if_is_dirty=False)


def floor_z():
    lo = 1e9
    for f in range(n):
        tt = dur * f / max(n - 1, 1)
        lo = min(lo, min(p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)))
    return lo


# pass 1 at ref height -> measure lowest foot; pass 2 lift so lowest foot = +MARGIN
write_hips(hp.z)
floor = floor_z()
base_z = hp.z - floor + ref_floor
write_hips(base_z)

lo = 1e9; hi = -1e9
for f in range(n):
    tt = dur * f / max(n - 1, 1)
    zs = [p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)]
    lo = min(lo, min(zs)); hi = max(hi, max(zs))
unreal.log("FIXED base=%.2f bob=+/-%.2fcm lowestFoot=%.2f height=%.1fcm" %
           (base_z, (max(az) - zmean) * SCALE, lo, hi - lo))
unreal.log("IMPORT FIX WALK DONE")
