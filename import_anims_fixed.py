import unreal

# Headless: import Walk+Idle animations against the existing skeleton,
# then re-apply the Hips root fix (the rig is metres; the skeleton ref pose
# carries x100 on the root which the FBX anim wipes -> ant-sized without this).
ART = "C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art"
SKEL = "/Game/Character/Hunyuan_Mesh_Skeleton"
JOBS = [(ART + "/Hunyuan_Walk.fbx", "A_Walk"),
        (ART + "/Hunyuan_Idle.fbx", "A_Idle")]

skeleton = unreal.load_asset(SKEL)
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
S = unreal.AnimSequenceService

for fbx, name in JOBS:
    ask = unreal.FbxAnimSequenceImportData()
    ask.set_editor_property("convert_scene", True)
    ask.set_editor_property("convert_scene_unit", True)
    ask.set_editor_property("import_uniform_scale", 1.0)
    ask.set_editor_property("remove_redundant_keys", False)
    ask.set_editor_property("animation_length",
        unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
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
    t.filename = fbx
    t.destination_path = "/Game/Character"
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = ui
    tools.import_asset_tasks([t])
    unreal.log("IMPORTED %s -> %s" % (name, list(t.imported_object_paths)))

# clean stray mesh/physics the anim-import sometimes spawns for new assets
for stray in ("/Game/Character/A_Idle_PhysicsAsset", "/Game/Character/A_Walk_PhysicsAsset"):
    if eal.does_asset_exist(stray):
        eal.delete_asset(stray)

# root fix on both
ref = {str(b.bone_name): b.transform for b in S.get_reference_pose(SKEL)}
hp = ref["Hips"].translation
for name in ("A_Walk", "A_Idle"):
    path = "/Game/Character/" + name
    anim = unreal.load_asset(path)
    if anim is None or anim.get_class().get_name() != "AnimSequence":
        unreal.log_warning("SKIP rootfix, not an AnimSequence: %s" % path)
        continue
    ctrl = anim.get_editor_property("controller")
    dm = ctrl.get_model_interface()
    n = dm.get_number_of_keys()
    rots = [S.get_bone_transform_at_frame(path, "Hips", f, False).rotation for f in range(n)]
    pos = [unreal.Vector(hp.x, hp.y, hp.z) for _ in range(n)]
    sca = [unreal.Vector(100.0, 100.0, 100.0) for _ in range(n)]
    ctrl.open_bracket("rootfix", True)
    ctrl.set_bone_track_keys("Hips", pos, rots, sca, True)
    ctrl.close_bracket(True)
    eal.save_asset(path, only_if_is_dirty=False)
    poses = S.get_pose_at_time(path, 0.0, True)
    zs = [p.transform.translation.z for p in poses]
    unreal.log("ROOTFIX %s keys=%d height=%.1fcm" % (name, n, max(zs) - min(zs)))

unreal.log("ANIM IMPORT DONE")
