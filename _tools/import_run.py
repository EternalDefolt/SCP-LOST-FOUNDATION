import unreal

# Import A_Run from the FBX, apply the metres-rig rootfix (force Hips scale 100,
# keep pos/rot), then ground the planted foot to the mesh bounds bottom (= world 0
# under the BodycamCharacter runtime placement), exactly like A_Walk/A_Idle.
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
SK = "/Game/Character/SK_Hunyuan"
path = "/Game/Character/A_Run"
S = unreal.AnimSequenceService
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
skeleton = unreal.load_asset(SKEL)

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
t.filename = ART + "/Hunyuan_Run.fbx"
t.destination_path = "/Game/Character"
t.destination_name = "A_Run"
t.automated = True
t.replace_existing = True
t.save = True
t.options = ui
tools.import_asset_tasks([t])
unreal.log("IMPORTED A_Run -> %s" % list(t.imported_object_paths))
if eal.does_asset_exist("/Game/Character/A_Run_PhysicsAsset"):
    eal.delete_asset("/Game/Character/A_Run_PhysicsAsset")

# rootfix: keep imported Hips pos/rot, force scale 100
anim = unreal.load_asset(path)
ctrl = anim.get_editor_property("controller")
n = ctrl.get_model_interface().get_number_of_keys()
pos, rots = [], []
for f in range(n):
    tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
    pos.append(tr.translation); rots.append(tr.rotation)
sca = [unreal.Vector(100.0, 100.0, 100.0) for _ in range(n)]
ctrl.open_bracket("rootfix", True)
ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
ctrl.close_bracket(True)
eal.save_asset(path, False)

# ground: shift Hips Z so the lowest foot over the cycle == mesh bounds bottom
sk = unreal.load_asset(SK)
B = sk.get_bounds()
target = B.origin.z - B.box_extent.z
dur = anim.get_editor_property("sequence_length")

def lowest():
    lo = 1e9
    for f in range(n):
        tt = dur * f / max(n - 1, 1)
        lo = min(lo, min(p.transform.translation.z for p in S.get_pose_at_time(path, tt, True)))
    return lo

lo = lowest()
shift = target - lo
pos2, rots2, sca2 = [], [], []
for f in range(n):
    tr = S.get_bone_transform_at_frame(path, "Hips", f, False)
    tt = tr.translation
    pos2.append(unreal.Vector(tt.x, tt.y, tt.z + shift))
    rots2.append(tr.rotation)
    sca2.append(unreal.Vector(100.0, 100.0, 100.0))
ctrl.open_bracket("ground", True)
ctrl.set_bone_track_keys("Hips", pos2, rots2, sca2, True)
ctrl.close_bracket(True)
ok = eal.save_asset(path, False)
unreal.log("A_Run grounded: target=%.2f wasLowest=%.2f shift=%.2f saveOK=%s nowLowest=%.2f" %
           (target, lo, shift, ok, lowest()))
unreal.log("IMPORT RUN DONE")
